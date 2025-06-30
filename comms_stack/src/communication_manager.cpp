#include "communication_manager.h"
#include "publisher.h"
#include "subscriber.h"
#include "rpc_client.h"
#include "rpc_service.h"

#include <vsomeip/vsomeip.hpp> // Main vsomeip header
#include <iostream>
#include <thread> // For std::this_thread::sleep_for if needed for shutdown
#include <chrono> // For std::chrono::milliseconds

// Forward declare or include actual protobuf message headers if used directly
// #include "common_messages.pb.h" // If we were to use SimpleNotification directly
#include "sample_rpc_service.pb.h" // For protos::SampleRpc
#include <google/protobuf/rpc/rpc.h> // For google::protobuf::Closure

// Placeholder Method IDs - these should ideally come from a shared configuration or generated code
// For now, ensure these are defined if not already visible from rpc_client.cpp (e.g. move to a common header)
#ifndef METHOD_ID_ECHO
#define METHOD_ID_ECHO 0x0001
#endif
#ifndef METHOD_ID_ADD
#define METHOD_ID_ADD  0x0002
#endif

namespace comms_stack {

CommunicationManager& CommunicationManager::getInstance() {
    static CommunicationManager instance;
    return instance;
}

CommunicationManager::CommunicationManager() : is_initialized_(false), vsomeip_app_(nullptr) {
    std::cout << "CommunicationManager: Constructor" << std::endl;
}

CommunicationManager::~CommunicationManager() {
    std::cout << "CommunicationManager: Destructor" << std::endl;
    if (is_initialized_ && vsomeip_app_ != nullptr) { // Check vsomeip_app_ as well
        shutdown();
    }
}

bool CommunicationManager::init(const std::string& app_name, const std::string& config_path) {
    if (is_initialized_) {
        std::cout << "CommunicationManager: Already initialized with app name: " << app_name_ << std::endl;
        return true;
    }
    app_name_ = app_name;
    std::cout << "CommunicationManager: Initializing for app: " << app_name_ << std::endl;

    // Set configuration path if provided and not empty
    // vsomeip primarily uses the VSOMEIP_CONFIGURATION environment variable.
    // If config_path is provided, we can try to set the environment variable programmatically,
    // but this is generally best set externally before the application starts.
    // Forcing it here might have limitations depending on the OS and how vsomeip reads it.
    if (!config_path.empty()) {
        // This is a common way to suggest a config file to vsomeip if the env var isn't set
        // Note: vsomeip might also look for a file named based on app_name in default locations.
        // The most reliable is VSOMEIP_CONFIGURATION.
        // Forcing environment variable from code:
        // setenv("VSOMEIP_CONFIGURATION", config_path.c_str(), 1); // For POSIX
        std::cout << "CommunicationManager: Custom config path provided: " << config_path
                  << ". Ensure VSOMEIP_CONFIGURATION is set or vsomeip can find it." << std::endl;
    }


    vsomeip_app_ = vsomeip::runtime::get()->create_application(app_name_);
    if (!vsomeip_app_) {
        std::cerr << "CommunicationManager: Failed to create vsomeip application." << std::endl;
        return false;
    }

    if (!vsomeip_app_->init()) {
        std::cerr << "CommunicationManager: Failed to initialize vsomeip application. Check configuration." << std::endl;
        vsomeip_app_.reset(); // Release the shared_ptr
        return false;
    }

    // Start the vsomeip application's dispatching threads
    // This call is non-blocking and starts internal threads in vsomeip.
    vsomeip_app_->start();
    std::cout << "CommunicationManager: vsomeip application started." << std::endl;

    is_initialized_ = true;
    std::cout << "CommunicationManager: Initialized successfully for app: " << app_name_ << std::endl;
    return true;
}

void CommunicationManager::shutdown() {
    if (!is_initialized_ || !vsomeip_app_) { // Check vsomeip_app_
        std::cout << "CommunicationManager: Not initialized or app already null, nothing to shut down." << std::endl;
        return;
    }
    std::cout << "CommunicationManager: Shutting down for app: " << app_name_ << "..." << std::endl;

    // 1. Clear caches and let Publishers/Subscribers/RpcClients/Services unregister themselves
    //    in their destructors. The order might matter if there are interdependencies.
    //    Clearing these shared_ptrs will trigger their destructors if their ref count becomes 0.
    std::cout << "CommunicationManager: Clearing RPC clients..." << std::endl;
    rpc_client_cache_.clear();
    std::cout << "CommunicationManager: Clearing subscribers..." << std::endl;
    subscriber_cache_.clear();
    std::cout << "CommunicationManager: Clearing publishers..." << std::endl;
    publisher_cache_.clear();
    std::cout << "CommunicationManager: Clearing RPC service registry..." << std::endl;
    rpc_service_registry_.clear(); // This should trigger RpcService wrappers to stop offering services

    // 2. Stop all vsomeip event offers and service advertisements (if not handled by above destructors)
    //    vsomeip_app_->clear_all_handler(); // Might be too aggressive, usually let objects manage their own state.
    //    vsomeip_app_->release_all_events();
    //    vsomeip_app_->unoffer_all_services();
    //    It's generally better practice for the individual Publisher/Subscriber/RpcService
    //    wrappers to clean up their specific vsomeip registrations in their destructors.
    //    The cache clearing above should trigger this.

    // 3. Stop the vsomeip application. This stops its internal threads.
    std::cout << "CommunicationManager: Stopping vsomeip application..." << std::endl;
    if (vsomeip_app_) {
        vsomeip_app_->stop(); // Stops dispatching, joins threads.
    }

    // Give a brief moment for threads to join, though vsomeip_app_->stop() should be synchronous.
    // std::this_thread::sleep_for(std::chrono::milliseconds(100));

    vsomeip_app_.reset(); // Release the shared_ptr

    is_initialized_ = false;
    std::cout << "CommunicationManager: Shutdown complete for app: " << app_name_ << std::endl;
}

std::shared_ptr<vsomeip::application> CommunicationManager::getVsomeipApplication() {
    return vsomeip_app_;
}

// Placeholder implementations for getPublisher, getSubscriber, etc.
// (These will be fleshed out in later steps but need to exist for linking)
std::shared_ptr<Publisher> CommunicationManager::getPublisher(const std::string& topic_name) {
    if (!is_initialized_ || !vsomeip_app_) {
        std::cerr << "CommunicationManager: Not initialized. Cannot get publisher." << std::endl;
        return nullptr;
    }
    auto it = publisher_cache_.find(topic_name);
    if (it != publisher_cache_.end()) {
        return it->second;
    }
    // Logic to create and cache publisher will be added later
    std::cout << "CommunicationManager: Getting publisher for topic: " << topic_name << " (placeholder - not fully implemented)." << std::endl;
    auto publisher = std::make_shared<Publisher>(topic_name, vsomeip_app_); // Pass app
    publisher_cache_[topic_name] = publisher;
    return publisher;
}

std::shared_ptr<Subscriber> CommunicationManager::getSubscriber(const std::string& topic_name) {
    if (!is_initialized_ || !vsomeip_app_) {
        std::cerr << "CommunicationManager: Not initialized. Cannot get subscriber." << std::endl;
        return nullptr;
    }
     auto it = subscriber_cache_.find(topic_name);
    if (it != subscriber_cache_.end()) {
        return it->second;
    }
    std::cout << "CommunicationManager: Getting subscriber for topic: " << topic_name << " (placeholder - not fully implemented)." << std::endl;
    auto subscriber = std::make_shared<Subscriber>(topic_name, vsomeip_app_); // Pass app
    subscriber_cache_[topic_name] = subscriber;
    return subscriber;
}

void CommunicationManager::registerRpcService(
    const std::string& user_service_name,
    uint16_t service_id,
    uint16_t instance_id,
    std::shared_ptr<protos::SampleRpc> service_impl) {

    if (!is_initialized_ || !vsomeip_app_) {
        std::cerr << "CommunicationManager: Not initialized. Cannot register RPC service " << user_service_name << std::endl;
        return;
    }
    if (!service_impl) {
        std::cerr << "CommunicationManager: Service implementation for " << user_service_name << " is null." << std::endl;
        return;
    }

    actual_rpc_services_[user_service_name] = service_impl; // Store it

    vsomeip_app_->offer_service(service_id, instance_id);
    std::cout << "CommunicationManager: Offered RPC service " << user_service_name
              << " (ID: 0x" << std::hex << service_id
              << ", Instance: 0x" << instance_id << std::dec << ")" << std::endl;

    // --- Register handler for Echo method ---
    vsomeip_app_->register_message_handler(
        service_id, vsomeip::ANY_INSTANCE, METHOD_ID_ECHO, // Listen on any instance for this service/method
        [this, service_impl](const std::shared_ptr<vsomeip::message>& req_msg) {
            std::cout << "RPC Server: Echo request received (Service: 0x" << std::hex << req_msg->get_service()
                      << ", Method: 0x" << req_msg->get_method()
                      << ", Client: 0x" << req_msg->get_client()
                      << ", Session: 0x" << req_msg->get_session() << std::dec << ")" << std::endl;

            protos::EchoRequest request;
            protos::EchoResponse response;

            auto payload = req_msg->get_payload();
            if (payload && payload->get_length() > 0) {
                if (!request.ParseFromArray(payload->get_data(), payload->get_length())) {
                    std::cerr << "RPC Server (Echo): Failed to parse request." << std::endl;
                    std::shared_ptr<vsomeip::message> err_res = vsomeip::runtime::get()->create_response(req_msg);
                    err_res->set_return_code(vsomeip::return_code_e::E_MALFORMED_MESSAGE);
                    vsomeip_app_->send(err_res);
                    return;
                }
            } else {
                 std::cerr << "RPC Server (Echo): Received empty payload." << std::endl;
                std::shared_ptr<vsomeip::message> err_res = vsomeip::runtime::get()->create_response(req_msg);
                err_res->set_return_code(vsomeip::return_code_e::E_MALFORMED_MESSAGE);
                vsomeip_app_->send(err_res);
                return;
            }

            ::google::protobuf::Closure* dummy_done = ::google::protobuf::NewCallback(
                [this, req_msg, &response]() {
                    std::string serialized_response;
                    if (!response.SerializeToString(&serialized_response)) {
                        std::cerr << "RPC Server (Echo): Failed to serialize response." << std::endl;
                        std::shared_ptr<vsomeip::message> err_res = vsomeip::runtime::get()->create_response(req_msg);
                        err_res->set_return_code(vsomeip::return_code_e::E_SERIALIZATION);
                        vsomeip_app_->send(err_res);
                        return;
                    }

                    std::shared_ptr<vsomeip::message> vsomeip_res = vsomeip::runtime::get()->create_response(req_msg);
                    std::shared_ptr<vsomeip::payload> res_payload = vsomeip::runtime::get()->create_payload();
                    std::vector<vsomeip::byte_t> res_payload_data(serialized_response.begin(), serialized_response.end());
                    res_payload->set_data(res_payload_data);
                    vsomeip_res->set_payload(res_payload);

                    vsomeip_app_->send(vsomeip_res);
                     std::cout << "RPC Server (Echo): Sent response." << std::endl;
                }
            );

            service_impl->Echo(nullptr, &request, &response, dummy_done);
        }
    );
     std::cout << "CommunicationManager: Registered handler for Echo method (0x" << std::hex << METHOD_ID_ECHO << std::dec << ")" << std::endl;

    // --- Register handler for Add method ---
    vsomeip_app_->register_message_handler(
        service_id, vsomeip::ANY_INSTANCE, METHOD_ID_ADD,
        [this, service_impl](const std::shared_ptr<vsomeip::message>& req_msg) {
            std::cout << "RPC Server: Add request received." << std::endl;
            protos::AddRequest request;
            protos::AddResponse response;

            auto payload = req_msg->get_payload();
            if (payload && payload->get_length() > 0) {
                if (!request.ParseFromArray(payload->get_data(), payload->get_length())) {
                    std::cerr << "RPC Server (Add): Failed to parse request." << std::endl;
                    std::shared_ptr<vsomeip::message> err_res = vsomeip::runtime::get()->create_response(req_msg);
                    err_res->set_return_code(vsomeip::return_code_e::E_MALFORMED_MESSAGE);
                    vsomeip_app_->send(err_res);
                    return;
                }
            } else {
                 std::cerr << "RPC Server (Add): Received empty payload." << std::endl;
                std::shared_ptr<vsomeip::message> err_res = vsomeip::runtime::get()->create_response(req_msg);
                err_res->set_return_code(vsomeip::return_code_e::E_MALFORMED_MESSAGE);
                vsomeip_app_->send(err_res);
                return;
            }

            ::google::protobuf::Closure* dummy_done = ::google::protobuf::NewCallback(
                [this, req_msg, &response]() {
                    std::string serialized_response;
                    if (!response.SerializeToString(&serialized_response)) {
                        std::cerr << "RPC Server (Add): Failed to serialize response." << std::endl;
                        std::shared_ptr<vsomeip::message> err_res = vsomeip::runtime::get()->create_response(req_msg);
                        err_res->set_return_code(vsomeip::return_code_e::E_SERIALIZATION);
                        vsomeip_app_->send(err_res);
                        return;
                    }
                    std::shared_ptr<vsomeip::message> vsomeip_res = vsomeip::runtime::get()->create_response(req_msg);
                    std::shared_ptr<vsomeip::payload> res_payload = vsomeip::runtime::get()->create_payload();
                     std::vector<vsomeip::byte_t> res_payload_data(serialized_response.begin(), serialized_response.end());
                    res_payload->set_data(res_payload_data);
                    vsomeip_res->set_payload(res_payload);
                    vsomeip_app_->send(vsomeip_res);
                    std::cout << "RPC Server (Add): Sent response." << std::endl;
                }
            );
            service_impl->Add(nullptr, &request, &response, dummy_done);
        }
    );
    std::cout << "CommunicationManager: Registered handler for Add method (0x" << std::hex << METHOD_ID_ADD << std::dec << ")" << std::endl;
}

std::shared_ptr<RpcClient> CommunicationManager::getRpcClient(const std::string& service_name) {
    if (!is_initialized_ || !vsomeip_app_) {
        std::cerr << "CommunicationManager: Not initialized. Cannot get RPC client." << std::endl;
        return nullptr;
    }
    auto it = rpc_client_cache_.find(service_name);
    if (it != rpc_client_cache_.end()) {
        return it->second;
    }
    std::cout << "CommunicationManager: Getting RPC client for service: " << service_name << " (placeholder - not fully implemented)." << std::endl;
    auto client = std::make_shared<RpcClient>(service_name, vsomeip_app_); // Pass app
    rpc_client_cache_[service_name] = client;
    return client;
}

} // namespace comms_stack
