#include "FovClient.h"

#include "ClientImpl.h"

#include "Fov.grpc.pb.h"



namespace {


#define MOVE_STUFF_MACRO(type, name) result.name = src.name();
#define MOVE_STUFF_PTR_MACRO(type, name) pResult->name = src.name();

auto AsPlainFoiImage(const Fov::Image& src)
{
    auto pResult = std::make_shared<PlainFoiImage>();
    FOI_IMAGE_X(MOVE_STUFF_PTR_MACRO)
    pResult->data = { src.data().begin(), src.data().end() };
    return pResult;
}

auto AsPlainFoiObject(const Fov::Object& src)
{
    PlainFoiObject result;
    FOI_OBJECT_X(MOVE_STUFF_MACRO)
    return result;
}

auto AsPlain(const Fov::Event& src)
{
    PlainFoiEvent result;
    FOI_EVENT_X(MOVE_STUFF_MACRO)

    result.image = AsPlainFoiImage(src.image());

    for (int i = 0; i < src.objects_size(); ++i)
    {
        result.objects.push_back(AsPlainFoiObject(src.objects(i)));
    }

    return result;
}

auto AsPlain(const Fov::Notify& src)
{
    PlainFoiNotify result;
    FOI_NOTIFY_X(MOVE_STUFF_MACRO)

    for (int i = 0; i < src.images_size(); ++i)
    {
        result.images.push_back(AsPlainFoiImage(src.images(i)));
    }

    return result;
}

#undef MOVE_STUFF_PTR_MACRO
#undef MOVE_STUFF_MACRO


//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////

// AsyncDownstreamingClientCall

typedef AsyncDownstreamingClientCall<Fov::Event, PublishSubscribeClientCallback> EventClientCall;

typedef AsyncDownstreamingClientCall<Fov::Notify, NotifyClientCallback> NotifyClientCall;


class PublishSubscribeClient : public ClientImpl
{
public:
    explicit PublishSubscribeClient(
        const std::string& targetIpAddress,
        PublishSubscribeClientCallback callback)
        : ClientImpl(targetIpAddress)
        , stub_(Fov::EventSubscriber::NewStub(channel_))
        , callback_(std::move(callback))
    {
    }

    void RequestNotification(const Fov::EventChannel& id)
    {
        new EventClientCall(id, this, callback_, stub_);
    }

private:
    // Out of the passed in Channel comes the stub, stored here, our view of the
    // server"s exposed services.
    std::unique_ptr<Fov::EventSubscriber::Stub> stub_;

    PublishSubscribeClientCallback callback_;
};

class NotifyClient : public ClientImpl
{
public:
    explicit NotifyClient(
        const std::string& targetIpAddress,
        NotifyClientCallback callback)
        : ClientImpl(targetIpAddress)
        , stub_(Fov::NotifySubscriber::NewStub(channel_))
        , callback_(std::move(callback))
    {
    }

    void RequestNotification(const Fov::NotifyChannel& id)
    {
        new NotifyClientCall(id, this, callback_, stub_);
    }

private:
    // Out of the passed in Channel comes the stub, stored here, our view of the
    // server"s exposed services.
    std::unique_ptr<Fov::NotifySubscriber::Stub> stub_;

    NotifyClientCallback callback_;
};


} // namespace

std::unique_ptr<IPublishSubscribeClient> MakePublishSubscribeClient(
    const std::string& targetIpAddress, const std::string& id, PublishSubscribeClientCallback callback)
{
    auto result = std::make_unique<PublishSubscribeClient>(targetIpAddress, callback);

    Fov::EventChannel request;
    request.set_id(id);
    result->RequestNotification(request);
    result->RunAsync();

    return result;
}

std::unique_ptr<IPublishSubscribeClient> MakeNotifyClient(
    const std::string& targetIpAddress, const std::string& id, NotifyClientCallback callback)
{
    auto result = std::make_unique<NotifyClient>(targetIpAddress, callback);

    Fov::NotifyChannel request;
    request.set_id(id);
    result->RequestNotification(request);
    result->RunAsync();

    return result;
}
