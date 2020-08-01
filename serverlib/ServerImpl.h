#pragma once

#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>
#include <grpcpp/alarm.h>
#include <grpcpp/impl/codegen/async_stream.h> // for grpc::ServerAsyncWriter

#include "Delegate.h"

#include <boost/signals2/signal.hpp>

#include <atomic>
#include <memory>
#include <queue>
#include <string>
#include <thread>


class ServerBase {
public: 
    virtual ~ServerBase() = default;
    virtual void RegisterService(grpc::ServerBuilder& builder) = 0;

    void setAlarm(grpc::Alarm& alarm, void* tag) {
        alarm.Set(cq_.get(), gpr_now(gpr_clock_type::GPR_CLOCK_REALTIME), tag);
    }

    std::unique_ptr<grpc::ServerCompletionQueue> cq_;
};


class CallData {
public:
    virtual ~CallData() = default;
    virtual void Proceed(bool ok) = 0;
};


class ServerImpl : public ServerBase {
public:
    ServerImpl(const std::string& serverIpAddress) : serverIpAddress_(serverIpAddress) {
    }

    ~ServerImpl() override {
        shutdownFlag_ = true;

        server_->Shutdown();
        // Always shutdown the completion queue after the server.
        cq_->Shutdown();

        // join
        thread_.join();

        // drain the queue
        void* ignoredTag = nullptr;
        bool ok = false;
        while (cq_->Next(&ignoredTag, &ok)) {
            ;
        }
    }

    void RunAsync() {
        thread_ = std::thread([this] { Run(); });
    }

protected:
    virtual void initCallData() = 0;

private:
    //friend class SubscriberCallData;

    // There is no shutdown handling in this code.
    void Run() {
        grpc::ServerBuilder builder;

        // https://cs.mcgill.ca/~mxia3/2019/02/23/Using-gRPC-in-Production/
        builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_TIME_MS, 10000);
        builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 5000);
        builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, true);
        builder.AddChannelArgument(GRPC_ARG_HTTP2_MAX_PINGS_WITHOUT_DATA, 0);
        builder.AddChannelArgument(GRPC_ARG_HTTP2_MIN_SENT_PING_INTERVAL_WITHOUT_DATA_MS, 10000);
        builder.AddChannelArgument(GRPC_ARG_HTTP2_MIN_RECV_PING_INTERVAL_WITHOUT_DATA_MS, 5000);

        builder.SetMaxMessageSize(GRPC_MAX_MESSAGE_SIZE);
        // Listen on the given address without any authentication mechanism.
        builder.AddListeningPort(serverIpAddress_, grpc::InsecureServerCredentials());
        // Register "service_" as the instance through which we'll communicate with
        // clients. In this case it corresponds to an *asynchronous* service.
        RegisterService(builder);

        // Get hold of the completion queue used for the asynchronous communication
        // with the gRPC runtime.
        cq_ = builder.AddCompletionQueue();
        // Finally assemble the server.
        server_ = builder.BuildAndStart();

        // Proceed to the server's main loop.
        HandleRpcs();
    }


    // This can be run in multiple threads if needed.
    void HandleRpcs() {
        // Spawn a new CallData instance to serve new clients.
        //new SubscriberCallData(this);

        initCallData();

        void* tag;  // uniquely identifies a request.
        bool ok;
        while (cq_->Next(&tag, &ok)) {
            // Block waiting to read the next event from the completion queue. The
            // event is uniquely identified by its tag, which in this case is the
            // memory address of a CallData instance.
            // The return value of Next should always be checked. This return value
            // tells us whether there is any kind of event or cq_ is shutting down.
            //GPR_ASSERT(ok);
            static_cast<CallData*>(tag)->Proceed(ok && !shutdownFlag_);
        }
    }
    
private:
    std::string serverIpAddress_;

    std::unique_ptr<grpc::Server> server_;

    std::thread thread_;

    std::atomic_bool shutdownFlag_ = false;
};



//////////////////////////////////////////////////////////////////////////////


// Class encompasing the state and logic needed to serve a request.
template <typename E, typename C, typename P, typename S>
class CallDataTemplate : public CallData {
public:
    // Take in the "service" instance (in this case representing an asynchronous
    // server) and the completion queue "cq" used for asynchronous communication
    // with the gRPC runtime.
    CallDataTemplate(boost::signals2::signal<void(const P&)>& observer, ServerBase* parent, S& service)
        : observer_(observer)
        , parent_(parent)
        , subscriberService_(service)
        , responder_(&ctx_), status_(CREATE) {
        // Invoke the serving logic right away.
        Proceed(true);
    }

    ~CallDataTemplate()
    {
        if (started_) {
            observer_.disconnect(MakeDelegate<&CallDataTemplate::HandleNotification>(this));
        }
    }

    void Proceed(bool ok) override {
        if (status_ == CREATE) {
            // Make this instance progress to the PROCESS state.
            status_ = PROCESS;

            // As part of the initial CREATE state, we *request* that the system
            // start processing SayHello requests. In this request, "this" acts are
            // the tag uniquely identifying the request (so that different CallData
            // instances can serve different requests concurrently), in this case
            // the memory address of this CallData instance.
            auto cq = parent_->cq_.get();
            subscriberService_.RequestSubscribe(&ctx_, &request_, &responder_, cq, cq, this);
        }
        else if (status_ == PROCESS) {
            // Spawn a new CallData instance to serve new clients while we process
            // the one for this CallData. The instance will deallocate itself as
            // part of its FINISH state.
            if (!started_)
            {
                if (!ok)
                {
                    delete this;
                    return;
                }

                new CallDataTemplate(observer_, parent_, subscriberService_);

                // subscribe to notifications
                observer_.connect(MakeDelegate<&CallDataTemplate::HandleNotification>(this));

                started_ = true;
            }

            // The actual processing.

            response_.Clear();

            // AsyncNotifyWhenDone?
            if (!ok)
            {
                status_ = FINISH;
                responder_.Finish(grpc::Status(), this);
            }
            else
            {
                bool hasNotification = false;
                {
                    std::lock_guard<std::mutex> locker(fifoMutex_);
                    if (!fifo_.empty())
                    {
                        hasNotification = true;
                        response_ = AsFoi(fifo_.front());
                        fifo_.pop();
                    }
                }

                if (hasNotification)
                {
                    responder_.Write(response_, this);
                    // https://www.gresearch.co.uk/2019/03/20/lessons-learnt-from-writing-asynchronous-streaming-grpc-services-in-c/
                    status_ = PUSH_TO_BACK;
                }
                else
                {
                    // https://www.gresearch.co.uk/2019/03/20/lessons-learnt-from-writing-asynchronous-streaming-grpc-services-in-c/
                    parent_->setAlarm(alarm_, this);
                }
            }
        }
        else if (status_ == PUSH_TO_BACK)
        {
            if (!ok)
            {
                status_ = FINISH;
                responder_.Finish(grpc::Status(), this);
            }
            else
            {
                status_ = PROCESS;
                parent_->setAlarm(alarm_, this);
            }
        }
        else {
            GPR_ASSERT(status_ == FINISH);
            // Once in the FINISH state, deallocate ourselves (CallData).
            delete this;
        }
    }

    void HandleNotification(const P& notification)
    {
        std::lock_guard<std::mutex> locker(fifoMutex_);
        fifo_.push(notification);
    }

private:
    boost::signals2::signal<void(const P&)>& observer_;

    // The means of communication with the gRPC runtime for an asynchronous server.
    // The producer-consumer queue where for asynchronous server notifications.
    ServerBase* parent_;

    S& subscriberService_;

    // Context for the rpc, allowing to tweak aspects of it such as the use
    // of compression, authentication, as well as to send metadata back to the client.
    grpc::ServerContext ctx_;

    // What we get from the client.
    C request_;

    // What we send back to the client.
    E response_;

    // The means to get back to the client.
    grpc::ServerAsyncWriter<E> responder_;

    // Let's implement a tiny state machine with the following states.
    enum CallStatus { CREATE, PROCESS, FINISH, PUSH_TO_BACK };
    CallStatus status_;  // The current serving state.

    std::queue<P> fifo_;
    std::mutex fifoMutex_;

    grpc::Alarm alarm_;

    bool started_ = false;
};

