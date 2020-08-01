#include "FovClient.h"
#include "FovServer.h"

#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"


#include "fqueue.h"

#include <signal.h>

#include <tuple>
#include <atomic>
#include <iostream>



static std::unique_ptr<IPublishSubscribeClient> client;

std::atomic_bool shutdownRequested(false);

void signalHandler(int signo)
{
    shutdownRequested = true;
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


inline size_t GetSize(const PlainFoiEvent& obj)
{
    return obj.image->data.size();
}

bool initCategory(PlainFoiObject & object, PlainFoiNotify &notification)
{
    if (object.score >= 0.01)
    {
        notification.category = object.label;
        notification.metric = object.score;
    }
    else
    {
        notification.category = "Unknown";
        notification.metric = 0.0;
    }

    return true;
}


int main()
{
    setSignalHandler();

    try {

        FQueue<PlainFoiEvent, 10 * 1024 * 1024, 10> queue;

        auto lam = [&queue](const PlainFoiEvent& notification) {
			auto &object = notification.objects[0];
            queue.push(notification);
        };
        client = MakePublishSubscribeClient("localhost:50051", "42", lam);

        auto server = MakeNotifyServer("0.0.0.0:50052");
		

        PlainFoiEvent event;
        while (!shutdownRequested && queue.pop(event))
        {
			PlainFoiNotify notification;

			notification.fov_id = event.fov_id;
			notification.sdu_id = event.sdu_id;
			notification.timestamp = event.timestamp;
			notification.coordinate = event.coordinate;
			notification.images.push_back(event.image);
			notification.status = 0;
			notification.frame_x      = event.objects[0].x;
			notification.frame_y      = event.objects[0].y;
			notification.frame_width  = event.objects[0].w;
			notification.frame_height = event.objects[0].h;

			auto full_image = cv::imdecode(event.image->data, cv::IMREAD_COLOR);

			auto &object = event.objects[0];


			cv::Rect roi = {object.x, object.y, object.w, object.h};

			auto image = full_image(roi);

			notification.object_width = object.w;
			notification.object_height = object.h;

			if(initCategory(object, notification))
			{
				notification.fov_id = std::to_string(
                    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

				server->Push(notification);

				cv::Mat frame;
				roi.x = std::max(0, roi.x - 100);
				roi.y = std::max(0, roi.y - 100);
				roi.width = (roi.x + roi.width + 200) < full_image.cols ? roi.width + 200 : full_image.cols - roi.x; 
				roi.height = (roi.y + roi.height + 200) < full_image.rows ? roi.height + 200 : full_image.rows - roi.y;
			}
		}           
    }
    catch (const std::exception& ex) {
        std::cerr << "Exception: " << typeid(ex).name() << ": " << ex.what();
        return EXIT_FAILURE;
    }
}
