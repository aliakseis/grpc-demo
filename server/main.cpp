#include "FovServer.h"


#include <cxxopts.hpp>

#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include <boost/iterator/transform_iterator.hpp>

#include <atomic>
#include <signal.h>
#include <thread>

#include <math.h>

#include <chrono>

#include <numeric>

#include <filesystem>
#include <fstream>
#include <functional>


namespace {

std::atomic_bool shutdownRequested(false);


void signalHandler(int signo)
{
    shutdownRequested = true;
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


std::pair<PlainFoiEvent, bool> GetStuff(const std::string& fname)
{
    using namespace std::chrono;

    cv::Mat frame = cv::imread(fname);
    if (frame.empty()) {
        return {};
    }

    PlainFoiEvent notification;

    const auto hash = std::hash_value(fname);
    const auto angle = hash % 91;
    const auto distance = hash % 37;
    const auto coord = std::to_string(angle) + ';' + std::to_string(distance);

    const auto timestamp = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    notification.fov_id = std::to_string(timestamp);
    notification.sdu_id = hash % 43;
    notification.timestamp = timestamp;
    notification.coordinate = coord; 

    std::ifstream f(fname, std::ios::binary);
    std::vector<char> v(std::istreambuf_iterator<char>{f}, {});

    auto image = std::make_shared<const PlainFoiImage>(PlainFoiImage{ frame.cols, frame.rows, std::move(v) });

    notification.image = image;

    notification.objects.push_back({ 
        0,
        0,
        frame.cols,
        frame.rows,
        108,
        static_cast<float>(frame.cols / 2),
        static_cast<float>(frame.rows / 2)
        });

    return { std::move(notification), true };
}

const int min_object_size = 100;
const int max_object_size = 50000;
const int max_count_anomalies = 250;

} // namespace

int main(int argc, char* argv[])
{
    try {
		
        cxxopts::Options options("FovServer", "FOV Server");

        options.add_options()
            ("a,addr", "IP Address", cxxopts::value<std::string>()->default_value("0.0.0.0:50051"))
            ("p,path", "Directory Path", cxxopts::value<std::string>()->default_value({}))
            ("s,sleep", "Sleep time between generations in seconds", cxxopts::value<int>()->default_value("1"))
            ;

        auto result = options.parse(argc, argv);

        setSignalHandler();

        auto folder = result["path"].as<std::string>();
        if (folder.empty()) {
            std::cout << "No directory path provided.\n";
        }

        auto server = MakePublishSubscribeServer(result["addr"].as<std::string>());

        const auto sleepTime = result["sleep"].as<int>();
				

		while(!shutdownRequested)
		{
            for (auto& p : std::filesystem::directory_iterator(folder))
            {
                if (p.is_regular_file())
                {
                    if (auto[notification, ok] = GetStuff(p.path().string()); ok) {
                        server->Push(notification);
                        std::this_thread::sleep_for(std::chrono::seconds(sleepTime));
                    }
                }
            }
		}
    }
    catch (const std::exception& ex) {
        std::cerr << "Exception " << typeid(ex).name() << ": " << ex.what() << '\n';
        return EXIT_FAILURE;
    }
}
