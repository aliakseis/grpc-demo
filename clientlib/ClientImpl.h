#pragma once

#include "IPublishSubscribeClient.h"

#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>
#include <grpcpp/impl/codegen/async_stream.h> // for grpc::ClientAsyncReader

#include "Delegate.h"

#include <boost/signals2/signal.hpp>

#include <atomic>
#include <memory>
#include <string>
#include <thread>

inline IPublishSubscribeClient::connectivity_state AsConnectivityState(grpc_connectivity_state state)
{
    switch (state) {
    case GRPC_CHANNEL_IDLE: return IPublishSubscribeClient::GRPC_CHANNEL_IDLE;
    case GRPC_CHANNEL_CONNECTING: return IPublishSubscribeClient::GRPC_CHANNEL_CONNECTING;
    case GRPC_CHANNEL_READY: return IPublishSubscribeClient::GRPC_CHANNEL_READY;
    case GRPC_CHANNEL_TRANSIENT_FAILURE: return IPublishSubscribeClient::GRPC_CHANNEL_TRANSIENT_FAILURE;
    case GRPC_CHANNEL_SHUTDOWN: return IPublishSubscribeClient::GRPC_CHANNEL_SHUTDOWN;
    }
}

inline auto GetChannelArguments()
{
    grpc::ChannelArguments result;
    result.SetMaxReceiveMessageSize(GRPC_MAX_MESSAGE_SIZE);

    // https://cs.mcgill.ca/~mxia3/2019/02/23/Using-gRPC-in-Production/
    std::pair<const char*, int> args[]{
        {GRPC_ARG_KEEPALIVE_TIME_MS,  10000},
        {GRPC_ARG_KEEPALIVE_TIMEOUT_MS,  5000},
        {GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS,  true},
        {GRPC_ARG_HTTP2_MAX_PINGS_WITHOUT_DATA,  0},
        {GRPC_ARG_HTTP2_MIN_SENT_PING_INTERVAL_WITHOUT_DATA_MS,  10000},
        {GRPC_ARG_HTTP2_MIN_RECV_PING_INTERVAL_WITHOUT_DATA_MS,  5000},
    };
    for (const auto& arg : args)
        result.SetInt(arg.first, arg.second);

    return result;
}

class ClientCallBase
{
public:
    virtual ~ClientCallBase() = default;
    virtual void Proceed(bool ok) = 0;
};


class ClientImpl : public IPublishSubscribeClient
{
protected:
    void AsyncCompleteRpc()
    {
        void* got_tag;
        bool ok = false;
        while (cq_.Next(&got_tag, &ok))
        {
            auto* call = static_cast<ClientCallBase*>(got_tag);
            call->Proceed(ok);
            if (numCalls_ == 0) {
                break;
            }
        }
    }
    void TryCancel() override
    {
        terminator_();
    }
    connectivity_state GetConnectionState(bool try_to_connect) override
    {
        return AsConnectivityState(channel_->GetState(try_to_connect));
    }

public:
    ClientImpl(const std::string& targetIpAddress)
        : channel_(grpc::CreateCustomChannel(
            targetIpAddress, grpc::InsecureChannelCredentials(), GetChannelArguments()))
    {
    }
    ~ClientImpl() override
    {
        terminator_();
        thread_.join();
    }

    void RunAsync()
    {
        thread_ = std::thread(&ClientImpl::AsyncCompleteRpc, this);
    }

    // The producer-consumer queue we use to communicate asynchronously with the
    // gRPC runtime.
    grpc::CompletionQueue cq_;
    std::atomic<int> numCalls_ = 0;
    boost::signals2::signal<void()> terminator_;

    std::thread thread_;

    std::shared_ptr<::grpc::Channel> channel_;
};



//////////////////////////////////////////////////////////////////////////////


// https://habr.com/ru/post/340758/
// https://github.com/Mityuha/grpc_async/blob/master/grpc_async_client.cc

template<typename E, typename C>
class AsyncDownstreamingClientCall : public ClientCallBase
{
    grpc::ClientContext context;
    E reply;
    grpc::Status status{};
    enum CallStatus { START, PROCESS, FINISH } callStatus;
    std::unique_ptr< grpc::ClientAsyncReader<E> > responder;

    ClientImpl* parent_;

    C& callback_;

public:
    template<typename R, typename T>
    AsyncDownstreamingClientCall(
        const R& request,
        ClientImpl* parent,
        C& callback,
        T& stub
    )
    : parent_(parent)
    , callback_(callback)
    {
        ++parent_->numCalls_;
        responder = stub->AsyncSubscribe(&context, request, &parent_->cq_, this);
        parent_->terminator_.connect(MakeDelegate<&grpc::ClientContext::TryCancel>(&context));
        callStatus = START;
    }
    ~AsyncDownstreamingClientCall()
    {
        parent_->terminator_.disconnect(MakeDelegate<&grpc::ClientContext::TryCancel>(&context));
        --parent_->numCalls_;
    }

    void Proceed(bool ok = true) override
    {
        switch (callStatus)
        {
        case PROCESS:
            // handle result
            // falls through
            if (ok)
            {
                callback_(AsPlain(reply));
            }
        case START:
            if (!ok)
            {
                responder->Finish(&status, this);
                gpr_log(GPR_INFO, "Status code: %d, message: \"%s\", details: \"%s\"", 
                    status.error_code(), status.error_message().c_str(), status.error_details().c_str());
                callStatus = FINISH;
                return;
            }
            callStatus = PROCESS;
            reply.Clear();
            responder->Read(&reply, this);
            break;
        case FINISH:
            delete this;
            break;
        }
    }
};
