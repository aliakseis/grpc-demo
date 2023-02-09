#include "FovClient.h"

#include "fqueue.h"

#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <signal.h>

#include <iostream>
#include <numeric>

namespace {

std::unique_ptr<IPublishSubscribeClient> client;

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

const char windowName[] = "FOV";

} // namespace

inline size_t GetSize(const PlainFoiNotify& obj)
{
    return std::accumulate(
        obj.images.begin(), obj.images.end(), 0u, [](size_t sum, auto& o) { return sum + o->data.size(); });
}

int main()
{
    setSignalHandler();

    try {
        FQueue<PlainFoiNotify, 10 * 1024 * 1024, 10> queue;

        cv::namedWindow(windowName, cv::WINDOW_NORMAL);

        auto lam = [&queue](const PlainFoiNotify& notification) {
            queue.push(notification);
        };
        client = MakeNotifyClient("localhost:50052", "42", lam);

        cv::Scalar clr{ 0, 0, 255 };

        PlainFoiNotify notification;
        while (queue.pop(notification))
        {
            const auto& image = notification.images[0];

            std::cout << notification.coordinate << ' ' << image->data.size() << '\n';
			auto frame = cv::imdecode(image->data, cv::IMREAD_COLOR);

            //for (auto& v : notification.objects)
            //{
                //std::cout << v.x << ' ' << v.y << ' ' << v.w << ' ' << v.h << '\n';
                cv::Rect rct(notification.frame_x, notification.frame_y, notification.frame_width, notification.frame_height);
                cv::rectangle(frame, rct, clr, 3);
				putText(frame, notification.category, cv::Point2f(notification.frame_x + 1, notification.frame_y - 1), cv::FONT_HERSHEY_SIMPLEX, 1, clr, 1);
            //}

            // Display the output image
			cv::resize(frame, frame, {frame.cols/2, frame.rows/2}, cv::INTER_NEAREST);
            cv::imshow(windowName, frame);

            // Break out of the loop if the user presses the Esc key
            char ch = cv::waitKey(20);
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
