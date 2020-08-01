#include "FovServer.h"

#include "ServerImpl.h"

#include "Fov.grpc.pb.h"


namespace {

#define MOVE_STUFF_MACRO(type, name) result.set_##name(src.name);
#define MOVE_STUFF_PTR_MACRO(type, name) pResult->set_##name(src.name);

auto AsFoiImage(const PlainFoiImage& src)
{
    auto pResult = std::make_unique<Fov::Image>();
    FOI_IMAGE_X(MOVE_STUFF_PTR_MACRO)
    pResult->set_data(src.data.data(), src.data.size());
    return pResult;
}

 void AsFoiObject(const PlainFoiObject& src, Fov::Object* pResult)
{
    FOI_OBJECT_X(MOVE_STUFF_PTR_MACRO)
}

Fov::Event AsFoi(const PlainFoiEvent& src)
{
    Fov::Event result;
    FOI_EVENT_X(MOVE_STUFF_MACRO)

    result.set_allocated_image(AsFoiImage(*src.image).release());

    for (const auto& v : src.objects)
    {
        AsFoiObject(v, result.add_objects());
    }

    return result;
}

Fov::Notify AsFoi(const PlainFoiNotify& src)
{
    Fov::Notify result;

    FOI_NOTIFY_X(MOVE_STUFF_MACRO)

    for (const auto& v : src.images)
    {
        const auto& src = *v;
        auto pResult = result.add_images();
        FOI_IMAGE_X(MOVE_STUFF_PTR_MACRO)
        pResult->set_data(src.data.data(), src.data.size());
    }

    return result;
}


#undef MOVE_STUFF_PTR_MACRO
#undef MOVE_STUFF_MACRO



// class SubscriberCallData
typedef CallDataTemplate<Fov::Event, Fov::EventChannel, PlainFoiEvent, Fov::EventSubscriber::AsyncService> EventSubscriberCallData;

typedef CallDataTemplate<Fov::Notify, Fov::NotifyChannel, PlainFoiNotify, Fov::NotifySubscriber::AsyncService> NotifySubscriberCallData;

//////////////////////////////////////////////////////////////////////////////


class PublishSubscribeServer : public IPublishSubscribeServer, public ServerImpl {
public:
    using ServerImpl::ServerImpl;


    void initCallData() override
    {
        new EventSubscriberCallData(observer_, this, subscriberService_);
    }

    void Push(const PlainFoiEvent& notification) override
    {
        observer_(notification);
    }

    void RegisterService(grpc::ServerBuilder& builder) override {
        builder.RegisterService(&subscriberService_);
    }

private:
    Fov::EventSubscriber::AsyncService subscriberService_;
    boost::signals2::signal<void(const PlainFoiEvent&)> observer_;
};

class NotifyServer : public INotifyServer, public ServerImpl {
public:
    using ServerImpl::ServerImpl;


    void initCallData() override
    {
        new NotifySubscriberCallData(observer_, this, subscriberService_);
    }

    void Push(const PlainFoiNotify& notification) override
    {
        observer_(notification);
    }

    void RegisterService(grpc::ServerBuilder& builder) override {
        builder.RegisterService(&subscriberService_);
    }

private:
    Fov::NotifySubscriber::AsyncService subscriberService_;
    boost::signals2::signal<void(const PlainFoiNotify&)> observer_;
};



} // namespace


std::unique_ptr<IPublishSubscribeServer> MakePublishSubscribeServer(const std::string& serverIpAddress)
{
    auto result = std::make_unique<PublishSubscribeServer>(serverIpAddress);
    result->RunAsync();
    return result;
}

std::unique_ptr<INotifyServer> MakeNotifyServer(const std::string& serverIpAddress)
{
    auto result = std::make_unique<NotifyServer>(serverIpAddress);
    result->RunAsync();
    return result;
}
