#include "FovClient.h"

#include "fqueue.h"

#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <grpc/support/log.h>

#include <signal.h>

#include <iostream>

namespace {

static std::unique_ptr<IPublishSubscribeClient> client;

void signalHandler(int signo)
{
    if (client) {
        client->TryCancel();
    }
}


void setSignalHandler()
{
#ifdef _WIN32
    signal(SIGINT, signalHandler);
#else
    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
#endif
}

static void test_callback(gpr_log_func_args* args) {
    std::cout << "File: \"" << args->file
        << "\" line: " << args->line
        << " severity: " << args->severity
        << " message: " << args->message << '\n';
}



const char windowName[] = "Anomalies";

} // namespace


inline size_t GetSize(const PlainFoiEvent& obj)
{
    return obj.image->data.size();
}


/*

https://medium.com/@EdgePress/how-to-interact-with-and-debug-a-grpc-server-c4bc30ddeb0b

Environment variables
You can troubleshoot and change how C gRPC works by setting a few environment variables. There is a
more complete reference of these here but the most important are GRPC_TRACE and GRPC_VERBOSITY.
GRPC_TRACE
This is a comma-separated list of tracers that provide additional insight into how gRPC C core is
processing requests, via debug-logs.
all can additionally be used to turn all traces on. Individual traces can be disabled by prefixing them
with -.
refcount will turn on all of the tracers for refcount debugging.
if list_tracers is present, then all of the available tracers will be printed when the program starts up.
Example:
export GRPC_TRACE=all,-pending_tags
This means “put a trace on all available operations except pending_tags”. There are quite a few traces you
can turn on and off to debug what part of the request is failing, or just see more detail about what is
happening. These will work for any library that uses the C core (for example ruby, node, python.)
Available tracers include:
*	api — traces api calls to the C core
*	bdp_estimator — traces behavior of bdp estimation logic
*	call_combiner — traces call combiner state
*	call_error — traces the possible errors contributing to final call status
*	channel — traces operations on the C core channel stack
*	client_channel — traces client channel activity, including resolve and load balancing policy interaction
*	combiner — traces combiner lock state
*	compression — traces compression operations
*	connectivity_state — traces connectivity state changes to channels
*	channel_stack_builder — traces information about channel stacks being built
*	executor — traces grpc’s internal thread pool (‘the executor’)
*	glb — traces the grpclb load balancer
*	http — traces state in the http2 transport engine
*	http2_stream_state — traces all http2 stream state mutations.
*	http1 — traces HTTP/1.x operations performed by gRPC
*	inproc — traces the in-process transport
*	flowctl — traces http2 flow control
*	op_failure — traces error information when failure is pushed onto a completion queue
*	pick_first — traces the pick first load balancing policy
*	plugin_credentials — traces plugin credentials
*	pollable_refcount — traces reference counting of ‘pollable’ objects (only in DEBUG)
*	resource_quota — trace resource quota objects internals
*	round_robin — traces the round_robin load balancing policy
*	queue_pluck
*	queue_timeout
*	server_channel — lightweight trace of significant server channel events
*	secure_endpoint — traces bytes flowing through encrypted channels
*	timer — timers (alarms) in the grpc internals
*	timer_check — more detailed trace of timer logic in grpc internals
*	transport_security — traces metadata about secure channel establishment
*	tcp — traces bytes in and out of a channel
*	tsi — traces tsi transport security
The following tracers will only run in binaries built in DEBUG mode. This is accomplished by
invoking CONFIG=dbg make <target>.
*	alarm_refcount — refcounting traces for grpc_alarm structure
*	metadata — tracks creation and mutation of metadata
*	closure — tracks closure creation, scheduling, and completion
*	pending_tags — traces still-in-progress tags on completion queues
*	polling — traces the selected polling engine
*	polling_api — traces the api calls to polling engine
*	queue_refcount
*	error_refcount
*	stream_refcount
*	workqueue_refcount
*	fd_refcount
*	cq_refcount
*	auth_context_refcount
*	security_connector_refcount
*	resolver_refcount
*	lb_policy_refcount
*	chttp2_refcount
GRPC_VERBOSITY
Default gRPC logging verbosity — one of:
*	DEBUG — log all gRPC messages
*	INFO — log INFO and ERROR message
*	ERROR — log only errors

*/


int main()
{
    setSignalHandler();

    gpr_set_log_verbosity(GPR_LOG_SEVERITY_INFO);
    gpr_set_log_function(test_callback);

    // https://medium.com/@EdgePress/how-to-interact-with-and-debug-a-grpc-server-c4bc30ddeb0b
    putenv("GRPC_VERBOSITY=error");
    putenv("GRPC_TRACE=http,http2_stream_state,connectivity_state");

    try {
        FQueue<PlainFoiEvent, 10 * 1024 * 1024, 10> queue;

        cv::namedWindow(windowName, cv::WINDOW_NORMAL);

        auto lam = [&queue](const PlainFoiEvent& notification) {
            queue.push(notification);
        };
        client = MakePublishSubscribeClient("localhost:50051", "42", lam);

        for (auto state = client->GetConnectionState(true)
            ; state != IPublishSubscribeClient::GRPC_CHANNEL_READY
            ; state = client->GetConnectionState(true))
        {
            if (state == IPublishSubscribeClient::GRPC_CHANNEL_TRANSIENT_FAILURE) {
                std::cerr << "Could not connect to the server.\n";
                return EXIT_FAILURE;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        cv::Scalar clr{ 255, 0, 255 };

        PlainFoiEvent notification;
        while (queue.pop(notification))
        {
            std::cout << notification.coordinate << ' ' << notification.image->data.size() << '\n';
            auto frame = cv::imdecode(notification.image->data, cv::IMREAD_COLOR);

            int i = 0;
            for (auto& v : notification.objects)
            {
                ++i;
                //std::cout << v.x << ' ' << v.y << ' ' << v.w << ' ' << v.h << '\n';
                cv::Rect rct(v.x, v.y, v.w, v.h);
                cv::rectangle(frame, rct, clr);

                auto index = std::to_string(i);
                cv::putText(
                    frame, index, cv::Point2f(v.x, v.y - 2),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, clr, 1);
            }

            // Display the output image
            imshow(windowName, frame);

            // Break out of the loop if the user presses the Esc key
            char ch = cv::waitKey(10);
            if (ch == 27) {
                break;
}
        }
    }
    catch (const std::exception& ex) {
        std::cerr << "Exception " << typeid(ex).name() << ": " << ex.what() << '\n';
        return EXIT_FAILURE;
    }
}
