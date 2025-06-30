#include "communication_manager.h"
#include "subscriber.h"
#include "common_messages.pb.h" // For SimpleNotification
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <cstdlib>

// Configuration values from vsomeip_host.json (for subscriber)
const uint16_t TEST_TOPIC_SERVICE_ID = 0x1111;
// Subscriber usually listens for ANY_INSTANCE of the service offering the event,
// unless a specific provider instance is required. For availability, it might watch a specific one or ANY.
const uint16_t SERVICE_INSTANCE_TO_MONITOR = 0x0001; // Instance offered by publisher_test
// const uint16_t SERVICE_INSTANCE_TO_MONITOR = 0xFFFF; // vsomeip::ANY_INSTANCE
const uint16_t TEST_TOPIC_EVENTGROUP_ID = 0x9100;

volatile bool keep_running = true;

void signal_handler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received." << std::endl;
    keep_running = false;
}

void on_message_received_cb(const comms_stack::protos::SimpleNotification& message) {
    std::cout << "Subscriber CB: Received SimpleNotification:" << std::endl;
    std::cout << "  ID: " << message.id() << std::endl;
    std::cout << "  Content: " << message.message_content() << std::endl;
    std::cout << "  Timestamp: " << message.timestamp() << std::endl;
    std::cout << "------------------------------------" << std::endl;
}

int main(int argc, char** argv) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Set VSOMEIP_CONFIGURATION if needed, similar to publisher_test
    // const char* config_path_env = std::getenv("VSOMEIP_CONFIGURATION");
    // if (!config_path_env) {
    //    setenv("VSOMEIP_CONFIGURATION", "../vsomeip_config/vsomeip_host.json", 1);
    // }


    comms_stack::CommunicationManager& comm_mgr = comms_stack::CommunicationManager::getInstance();
    // Use the same app name as publisher if they are part of the same "ECU" concept for routing,
    // or a different one if it's a separate conceptual ECU. Both must be in JSON.
    if (!comm_mgr.init("CommsStackApp_PubSub")) {
        std::cerr << "Failed to initialize CommunicationManager" << std::endl;
        return 1;
    }

    auto sub_app = comm_mgr.getVsomeipApplication();
    if (!sub_app) {
         std::cerr << "Failed to get vsomeip application from comm_mgr" << std::endl;
         comm_mgr.shutdown();
         return 1;
    }

    // For subscriber, instance_id in constructor is the instance of the service to monitor for availability.
    // Our Subscriber implementation uses this instance_id for both availability and message handling/requesting.
    // This means it will only get events from that specific instance.
    // To get events from ANY instance offering the service/event, the Subscriber class would need
    // to use vsomeip::ANY_INSTANCE in register_message_handler and request_event.
    auto test_subscriber = std::make_shared<comms_stack::Subscriber>(
        "TestTopic",
        sub_app,
        TEST_TOPIC_SERVICE_ID,
        SERVICE_INSTANCE_TO_MONITOR,
        TEST_TOPIC_EVENTGROUP_ID,
        true // It's an eventgroup
    );

    if (!test_subscriber->subscribe(on_message_received_cb)) {
        std::cerr << "Failed to subscribe to TestTopic" << std::endl;
        comm_mgr.shutdown();
        return 1;
    }
    std::cout << "Subscribed to TestTopic. Waiting for messages..." << std::endl;

    int loop_count = 0;
    while (keep_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        loop_count++;
        if (argc > 1 && std::string(argv[1]) == "short" && loop_count > 20) { // Exit after 10s for CI
             std::cout << "Short run requested, exiting subscriber." << std::endl;
            break;
        }
         if (loop_count > 2000) break; // Safety break
    }

    std::cout << "Subscriber shutting down..." << std::endl;
    test_subscriber.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    comm_mgr.shutdown();
    std::cout << "Subscriber finished." << std::endl;
    return 0;
}
