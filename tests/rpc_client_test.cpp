#include "communication_manager.h"
#include "rpc_client.h"
#include "sample_rpc_service.pb.h" // For request/response protos
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <future> // For std::future status
#include <stdexcept> // For std::runtime_error

// Config values from vsomeip_host.json for SampleRpc client
const uint16_t RPC_SERVICE_ID = 0x2222;
const uint16_t RPC_INSTANCE_ID = 0x0001; // Instance ID the server offers

volatile bool keep_running = true;

void signal_handler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received." << std::endl;
    keep_running = false;
}

void make_echo_call(comms_stack::RpcClient& client, const std::string& message) {
    comms_stack::protos::EchoRequest req;
    req.set_request_message(message);
    std::cout << "RPC Client: Calling Echo with message: \"" << req.request_message() << "\"" << std::endl;

    std::future<comms_stack::protos::EchoResponse> echo_future = client.Echo(req);

    std::cout << "RPC Client: Waiting for Echo response..." << std::endl;
    if (echo_future.wait_for(std::chrono::seconds(5)) == std::future_status::timeout) {
        std::cerr << "RPC Client: Echo call timed out!" << std::endl;
    } else {
        try {
            comms_stack::protos::EchoResponse res = echo_future.get();
            std::cout << "RPC Client: Echo Response: \"" << res.response_message() << "\"" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "RPC Client: Echo call failed with exception: " << e.what() << std::endl;
        }
    }
}

void make_add_call(comms_stack::RpcClient& client, int32_t a, int32_t b) {
    comms_stack::protos::AddRequest req;
    req.set_a(a);
    req.set_b(b);
    std::cout << "RPC Client: Calling Add with a=" << req.a() << ", b=" << req.b() << std::endl;

    std::future<comms_stack::protos::AddResponse> add_future = client.Add(req);

    std::cout << "RPC Client: Waiting for Add response..." << std::endl;
    if (add_future.wait_for(std::chrono::seconds(5)) == std::future_status::timeout) {
        std::cerr << "RPC Client: Add call timed out!" << std::endl;
    } else {
        try {
            comms_stack::protos::AddResponse res = add_future.get();
            std::cout << "RPC Client: Add Response: sum=" << res.sum() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "RPC Client: Add call failed with exception: " << e.what() << std::endl;
        }
    }
}


int main(int argc, char** argv) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Set VSOMEIP_CONFIGURATION if needed
    // const char* config_path_env = std::getenv("VSOMEIP_CONFIGURATION");
    // if (!config_path_env) {
    //    setenv("VSOMEIP_CONFIGURATION", "../vsomeip_config/vsomeip_host.json", 1);
    // }

    comms_stack::CommunicationManager& comm_mgr = comms_stack::CommunicationManager::getInstance();
    if (!comm_mgr.init("CommsStackApp_RpcClient")) { // Use app name from JSON
        std::cerr << "Failed to initialize CommunicationManager for RPC Client" << std::endl;
        return 1;
    }

    // Similar to publisher, CommunicationManager::getRpcClient would ideally look up IDs.
    // For test, constructing directly.
    auto rpc_app = comm_mgr.getVsomeipApplication();
    if(!rpc_app) {
        std::cerr << "Failed to get vsomeip application from comm_mgr for RPC client" << std::endl;
        comm_mgr.shutdown();
        return 1;
    }
    auto rpc_client = std::make_shared<comms_stack::RpcClient>(
        "SampleRpc", // User-friendly name
        rpc_app,
        RPC_SERVICE_ID,
        RPC_INSTANCE_ID
    );

    std::cout << "RPC Client created. Waiting for service to become available..." << std::endl;
    // Wait for service availability
    int wait_count = 0;
    while(!rpc_client->isServiceAvailable() && wait_count < 10 && keep_running) { // Wait up to 5s
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        wait_count++;
    }

    if (!rpc_client->isServiceAvailable()) {
        std::cerr << "RPC Client: Service 'SampleRpc' did not become available. Exiting." << std::endl;
        rpc_client.reset();
        comm_mgr.shutdown();
        return 1;
    }
    std::cout << "RPC Client: Service 'SampleRpc' is available." << std::endl;

    if(keep_running) make_echo_call(*rpc_client, "Hello RPC World!");
    if(keep_running) std::this_thread::sleep_for(std::chrono::seconds(1));
    if(keep_running) make_add_call(*rpc_client, 10, 32);
    if(keep_running) std::this_thread::sleep_for(std::chrono::seconds(1));
    if(keep_running) make_echo_call(*rpc_client, "Another call");


    if (argc > 1 && std::string(argv[1]) == "short") {
        std::cout << "Short run requested, exiting RPC client." << std::endl;
    } else {
        // Keep running for a bit for more manual interaction if needed
        wait_count = 0;
        while(keep_running && wait_count < 20) { // Run for 10 more seconds
             std::this_thread::sleep_for(std::chrono::milliseconds(500));
             wait_count++;
        }
    }


    std::cout << "RPC Client shutting down..." << std::endl;
    rpc_client.reset(); // Explicitly destroy before comm_mgr shutdown
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    comm_mgr.shutdown();
    std::cout << "RPC Client finished." << std::endl;
    return 0;
}
