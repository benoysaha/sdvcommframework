#include "communication_manager.h"
#include "my_sample_rpc_impl.h" // The concrete implementation
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <cstdlib>

// Config values from vsomeip_host.json for SampleRpc
const uint16_t RPC_SERVICE_ID = 0x2222;
const uint16_t RPC_INSTANCE_ID = 0x0001; // Instance ID the server will offer

volatile bool keep_running = true;

void signal_handler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received." << std::endl;
    keep_running = false;
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
    if (!comm_mgr.init("CommsStackApp_RpcServer")) { // Use app name from JSON
        std::cerr << "Failed to initialize CommunicationManager for RPC Server" << std::endl;
        return 1;
    }

    auto rpc_service_impl = std::make_shared<comms_stack::MySampleRpcImpl>();

    // The CommunicationManager::registerRpcService takes the specific generated type
    comm_mgr.registerRpcService(
        "SampleRpc", // User-friendly name, used as key in manager's map
        RPC_SERVICE_ID,
        RPC_INSTANCE_ID,
        rpc_service_impl
    );

    std::cout << "RPC Server (SampleRpc) registered and offered. Waiting for requests..." << std::endl;

    int loop_count = 0;
    while (keep_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        loop_count++;
         if (argc > 1 && std::string(argv[1]) == "short" && loop_count > 20) { // Exit after 10s for CI
             std::cout << "Short run requested, exiting RPC server." << std::endl;
            break;
        }
        if (loop_count > 2000) break; // Safety break
    }

    std::cout << "RPC Server shutting down..." << std::endl;
    // Unregistration of service and handlers should happen when
    // CommunicationManager's reference to the service_impl (or its internal wrapper) is cleared,
    // or explicitly if CommunicationManager provided an unregisterRpcService method.
    // For now, relies on CommunicationManager::shutdown() clearing its maps, which should
    // lead to vsomeip unoffering and unregistering handlers if designed that way.
    // Our current registerRpcService directly registers handlers tied to the service_impl lifetime.
    // If `actual_rpc_services_` map in CommMgr is cleared, the shared_ptr count to `service_impl` might drop,
    // but the lambdas capturing `service_impl` for vsomeip handlers will keep it alive until
    // vsomeip itself unregisters those handlers (e.g. during app->stop()).

    // To be very clean, an unregister method would be good, or ensure CommunicationManager's
    // shutdown explicitly unregisters handlers.
    // For now, comm_mgr.shutdown() will stop the app, which stops handlers.
    comm_mgr.shutdown(); // This will call stop on the vsomeip_app
    std::cout << "RPC Server finished." << std::endl;
    return 0;
}
