#include "communication_manager.h"
#include "publisher.h"
#include "common_messages.pb.h" // For SimpleNotification
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <cstdlib> // For setenv if used, or use platform specific

// Configuration values from vsomeip_host.json (for publisher)
const uint16_t TEST_TOPIC_SERVICE_ID = 0x1111;
const uint16_t TEST_TOPIC_INSTANCE_ID = 0x0001; // Publisher offers this instance
const uint16_t TEST_TOPIC_EVENTGROUP_ID = 0x9100; // This is the eventgroup

volatile bool keep_running = true;

void signal_handler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received." << std::endl;
    keep_running = false;
}

int main(int argc, char** argv) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // This is crucial for vsomeip to find the configuration.
    // Replace "path/to/your/project/vsomeip_config/vsomeip_host.json"
    // with the actual absolute path or a relative path if the CWD is correct.
    // For CI/testing, it's better if the test runner sets this.
    // const char* config_path_env = std::getenv("VSOMEIP_CONFIGURATION");
    // if (!config_path_env) {
    //     std::cout << "VSOMEIP_CONFIGURATION not set, attempting to set to default for test." << std::endl;
    //     // On Linux/macOS:
    //     setenv("VSOMEIP_CONFIGURATION", "../vsomeip_config/vsomeip_host.json", 1);
    //     // On Windows, use _putenv_s
    //     // This path is relative to where the executable might be run from (e.g., build/tests)
    // } else {
    //      std::cout << "VSOMEIP_CONFIGURATION is: " << config_path_env << std::endl;
    // }


    comms_stack::CommunicationManager& comm_mgr = comms_stack::CommunicationManager::getInstance();
    if (!comm_mgr.init("CommsStackApp_PubSub")) { // Use app name from JSON
        std::cerr << "Failed to initialize CommunicationManager" << std::endl;
        return 1;
    }

     auto pub_app = comm_mgr.getVsomeipApplication();
     if (!pub_app) {
         std::cerr << "Failed to get vsomeip application from comm_mgr" << std::endl;
         comm_mgr.shutdown();
         return 1;
     }

    auto test_publisher = std::make_shared<comms_stack::Publisher>(
        "TestTopic",
        pub_app,
        TEST_TOPIC_SERVICE_ID,
        TEST_TOPIC_INSTANCE_ID,
        TEST_TOPIC_EVENTGROUP_ID,
        true // It's an eventgroup
    );

    std::cout << "Waiting for publisher to offer..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (!test_publisher->isOffered()) {
         std::cout << "TestTopic event/group not offered yet, waiting a bit more..." << std::endl;
         std::this_thread::sleep_for(std::chrono::seconds(2));
         if (!test_publisher->isOffered()){
            std::cerr << "Publisher failed to offer TestTopic. Exiting." << std::endl;
            // test_publisher will be destroyed by shared_ptr going out of scope
            comm_mgr.shutdown();
            return 1;
         }
    }
    std::cout << "Publisher has offered the event/group." << std::endl;


    uint32_t counter = 0;
    while (keep_running) {
        comms_stack::protos::SimpleNotification msg;
        msg.set_id(counter);
        std::string content = "Hello from Publisher! Count: " + std::to_string(counter);
        msg.set_message_content(content);
        msg.set_timestamp(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

        std::cout << "Publishing: ID=" << msg.id() << ", Message='" << msg.message_content() << "'" << std::endl;
        if(!test_publisher->publish(msg)){
            std::cerr << "Failed to publish message!" << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
        counter++;
        if (argc > 1 && std::string(argv[1]) == "short" && counter > 3) {
             std::cout << "Short run requested, exiting publisher." << std::endl;
            break;
        }
         if (counter > 1000) break; // Safety break for long runs
    }

    std::cout << "Publisher shutting down..." << std::endl;
    // test_publisher goes out of scope, its destructor will call stop_offer_event
    // Explicitly reset to control destruction order before comm_mgr.shutdown()
    test_publisher.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Give a moment for unoffer
    comm_mgr.shutdown();
    std::cout << "Publisher finished." << std::endl;
    return 0;
}
