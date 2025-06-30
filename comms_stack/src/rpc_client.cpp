#include "rpc_client.h"
#include "sample_rpc_service.pb.h" // For request/response types
#include <vsomeip/vsomeip.hpp>
#include <iostream>
#include <vector> // For payload data
#include <stdexcept> // For std::runtime_error

// Placeholder Method IDs - these should come from configuration
#define METHOD_ID_ECHO 0x0001
#define METHOD_ID_ADD  0x0002

namespace comms_stack {

RpcClient::RpcClient(const std::string& service_name,
                     std::shared_ptr<vsomeip::application> app,
                     uint16_t service_id,
                     uint16_t instance_id)
    : service_name_(service_name),
      vsomeip_app_(app),
      service_id_(service_id),
      instance_id_(instance_id),
      service_available_(false) {

    if (!vsomeip_app_) {
        std::cerr << "RpcClient (" << service_name_ << "): vsomeip application is null!" << std::endl;
        return;
    }
    client_id_ = vsomeip_app_->get_client(); // Get the client ID assigned by vsomeip

    std::cout << "RpcClient: Created for service: " << service_name_
              << " (Service ID: 0x" << std::hex << service_id_
              << ", Instance ID: 0x" << instance_id_
              << ", Client ID: 0x" << client_id_ << std::dec << ")" << std::endl;

    vsomeip_app_->register_availability_handler(
        service_id_, instance_id_,
        std::bind(&RpcClient::onAvailabilityChanged, this,
                  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
    );

    // Register a general message handler for this client.
    // All RPC responses will come through this handler.
    // We differentiate them by session ID.
    vsomeip_app_->register_message_handler(
        vsomeip::ANY_SERVICE, vsomeip::ANY_INSTANCE, vsomeip::ANY_METHOD, // Or more specific if desired
        std::bind(&RpcClient::onMessageReceived, this, std::placeholders::_1)
    );
    // It's often better to register for service_id_, ANY_INSTANCE, ANY_METHOD
    // to only get messages related to the service this client is for.
    // vsomeip_app_->register_message_handler(
    //     service_id_, vsomeip::ANY_INSTANCE, vsomeip::ANY_METHOD,
    //     std::bind(&RpcClient::onMessageReceived, this, std::placeholders::_1));


    // Requesting the service makes vsomeip try to find it.
    // Method calls will also trigger discovery if not found yet.
    vsomeip_app_->request_service(service_id_, instance_id_);
}

RpcClient::~RpcClient() {
    std::cout << "RpcClient: Destroyed for service: " << service_name_ << std::endl;
    if (vsomeip_app_) {
        vsomeip_app_->unregister_availability_handler(
            service_id_, instance_id_,
            std::bind(&RpcClient::onAvailabilityChanged, this,
                      std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        // Unregister the general message handler (if it was specific to this client's needs)
        // If it was a truly global handler in CommunicationManager, it wouldn't be unregistered here.
        // Assuming we registered one per RpcClient instance for its service_id:
         vsomeip_app_->unregister_message_handler(
            vsomeip::ANY_SERVICE, vsomeip::ANY_INSTANCE, vsomeip::ANY_METHOD, // Match registration
            std::bind(&RpcClient::onMessageReceived, this, std::placeholders::_1));
        // Or:
        // vsomeip_app_->unregister_message_handler(
        //    service_id_, vsomeip::ANY_INSTANCE, vsomeip::ANY_METHOD,
        //    std::bind(&RpcClient::onMessageReceived, this, std::placeholders::_1));


        vsomeip_app_->release_service(service_id_, instance_id_);

        // Cancel any pending promises
        std::lock_guard<std::mutex> lock(pending_requests_mutex_);
        for (auto const& [key, val] : pending_requests_) {
            // How to properly notify promise depends on what's stored in PromiseContext
            // For now, just log. A real implementation would set_exception.
             std::cerr << "RpcClient (" << service_name_
                       << "): Unfulfilled promise for client/session: 0x" << std::hex << key.first
                       << "/0x" << key.second << std::dec << " on destruction." << std::endl;
        }
        pending_requests_.clear();
    }
}

template<typename ResProto>
void RpcClient::registerPromise(vsomeip::client_t client_id, vsomeip::session_t session_id, std::promise<ResProto> promise) {
    std::lock_guard<std::mutex> lock(pending_requests_mutex_);
    pending_requests_[{client_id, session_id}] = {
        // Store a lambda that knows how to parse into ResProto and set the promise
        [this, p = std::move(promise)](const std::shared_ptr<vsomeip::message>& msg) mutable {
            ResProto response_proto;
            if (msg->get_return_code() != vsomeip::return_code_e::E_OK) {
                std::string error_msg = "RPC Error: Received non-OK return code: " + std::to_string(static_cast<int>(msg->get_return_code()));
                 std::cerr << "RpcClient (" << service_name_ << "): " << error_msg << std::endl;
                try { p.set_exception(std::make_exception_ptr(std::runtime_error(error_msg))); } catch(...) {} // set_exception might throw
                return;
            }

            auto payload = msg->get_payload();
            if (!payload || payload->get_length() == 0) {
                std::string error_msg = "RPC Error: Received empty payload for response.";
                std::cerr << "RpcClient (" << service_name_ << "): " << error_msg << std::endl;
                try { p.set_exception(std::make_exception_ptr(std::runtime_error(error_msg))); } catch(...) {}
                return;
            }
            if (response_proto.ParseFromArray(payload->get_data(), payload->get_length())) {
                try { p.set_value(response_proto); } catch(...) {}
            } else {
                 std::string error_msg = "RPC Error: Failed to parse response payload into " + response_proto.GetTypeName();
                 std::cerr << "RpcClient (" << service_name_ << "): " << error_msg << std::endl;
                try { p.set_exception(std::make_exception_ptr(std::runtime_error(error_msg))); } catch(...) {}
            }
        }
    };
}

template<typename ResProto>
void RpcClient::fulfillPromise(vsomeip::client_t client_id, vsomeip::session_t session_id, const std::shared_ptr<vsomeip::message>& msg) {
    std::lock_guard<std::mutex> lock(pending_requests_mutex_);
    auto key = std::make_pair(client_id, session_id);
    auto it = pending_requests_.find(key);
    if (it != pending_requests_.end()) {
        it->second.response_parser(msg); // Call the stored lambda
        pending_requests_.erase(it);
    } else {
        std::cerr << "RpcClient (" << service_name_
                  << "): Received response for unknown client/session: 0x" << std::hex << client_id
                  << "/0x" << session_id << std::dec << std::endl;
    }
}

void RpcClient::fulfillPromiseWithError(vsomeip::client_t client_id, vsomeip::session_t session_id, const std::string& error_msg) {
    std::lock_guard<std::mutex> lock(pending_requests_mutex_);
    auto key = std::make_pair(client_id, session_id);
    auto it = pending_requests_.find(key);
    if (it != pending_requests_.end()) {
        // This is tricky because the stored promise type is erased by std::function.
        // We'd need to store std::shared_ptr<void> to the promise and type-erase it,
        // or store a std::function<void(const std::string&)> to set an error.
        // For simplicity now, just log and erase. A real system needs robust error propagation.
        std::cerr << "RpcClient (" << service_name_ << "): Setting error for client/session 0x"
                  << std::hex << client_id << "/0x" << session_id << ": " << error_msg << std::dec << std::endl;
        // it->second.promise_setter_with_error(error_msg); // If we had such a mechanism
        pending_requests_.erase(it); // Or let the original call timeout
    }
}


std::future<protos::EchoResponse> RpcClient::Echo(const protos::EchoRequest& request) {
    std::promise<protos::EchoResponse> promise;
    auto future = promise.get_future();

    if (!vsomeip_app_ || !service_available_) {
        std::cerr << "RpcClient (" << service_name_ << "): Cannot call Echo, app not ready or service unavailable." << std::endl;
        promise.set_exception(std::make_exception_ptr(std::runtime_error("Service not available or app not ready")));
        return future;
    }

    std::shared_ptr<vsomeip::message> rpc_request = vsomeip::runtime::get()->create_request();
    rpc_request->set_service(service_id_);
    rpc_request->set_instance(instance_id_);
    rpc_request->set_method(METHOD_ID_ECHO); // Placeholder

    std::string serialized_data;
    if (!request.SerializeToString(&serialized_data)) {
        std::cerr << "RpcClient (" << service_name_ << "): Failed to serialize EchoRequest." << std::endl;
        promise.set_exception(std::make_exception_ptr(std::runtime_error("Failed to serialize request")));
        return future;
    }

    std::shared_ptr<vsomeip::payload> payload = vsomeip::runtime::get()->create_payload();
    std::vector<vsomeip::byte_t> payload_data(serialized_data.begin(), serialized_data.end());
    payload->set_data(payload_data);
    rpc_request->set_payload(payload);

    // Store promise before sending, using client_id and generated session_id
    registerPromise<protos::EchoResponse>(rpc_request->get_client(), rpc_request->get_session(), std::move(promise));

    vsomeip_app_->send(rpc_request);
    std::cout << "RpcClient (" << service_name_ << "): Sent Echo request (Session: 0x"
              << std::hex << rpc_request->get_session() << std::dec << ")" << std::endl;
    return future;
}

std::future<protos::AddResponse> RpcClient::Add(const protos::AddRequest& request) {
    std::promise<protos::AddResponse> promise;
    auto future = promise.get_future();

    if (!vsomeip_app_ || !service_available_) {
        std::cerr << "RpcClient (" << service_name_ << "): Cannot call Add, app not ready or service unavailable." << std::endl;
        promise.set_exception(std::make_exception_ptr(std::runtime_error("Service not available or app not ready")));
        return future;
    }

    std::shared_ptr<vsomeip::message> rpc_request = vsomeip::runtime::get()->create_request();
    rpc_request->set_service(service_id_);
    rpc_request->set_instance(instance_id_);
    rpc_request->set_method(METHOD_ID_ADD); // Placeholder

    std::string serialized_data;
    if (!request.SerializeToString(&serialized_data)) {
        std::cerr << "RpcClient (" << service_name_ << "): Failed to serialize AddRequest." << std::endl;
        promise.set_exception(std::make_exception_ptr(std::runtime_error("Failed to serialize request")));
        return future;
    }

    std::shared_ptr<vsomeip::payload> payload = vsomeip::runtime::get()->create_payload();
    std::vector<vsomeip::byte_t> payload_data(serialized_data.begin(), serialized_data.end());
    payload->set_data(payload_data);
    rpc_request->set_payload(payload);

    registerPromise<protos::AddResponse>(rpc_request->get_client(), rpc_request->get_session(), std::move(promise));

    vsomeip_app_->send(rpc_request);
    std::cout << "RpcClient (" << service_name_ << "): Sent Add request (Session: 0x"
              << std::hex << rpc_request->get_session() << std::dec << ")" << std::endl;
    return future;
}

void RpcClient::onAvailabilityChanged(vsomeip::service_t service, vsomeip::instance_t instance, bool is_available) {
    if (service == service_id_ && instance == instance_id_) {
        service_available_ = is_available;
        std::cout << "RpcClient (" << service_name_ << "): Service availability changed for Service 0x"
                  << std::hex << service << ", Instance 0x" << instance
                  << " -> " << (is_available ? "AVAILABLE" : "NOT AVAILABLE") << std::dec << std::endl;

        if (!is_available) {
            // Service went down, fail any pending requests for this service
            std::lock_guard<std::mutex> lock(pending_requests_mutex_);
            // This simple iteration won't work directly with fulfillPromiseWithError's current signature
            // as we don't have client/session. A more robust way is needed, e.g. storing service_id
            // with promises or iterating and checking.
            // For now, this is a conceptual cleanup.
            std::cout << "RpcClient (" << service_name_ << "): Service became unavailable. Pending requests might fail." << std::endl;
            // A better approach: iterate pending_requests_ and set_exception on promises
            // that were targeting this service_id/instance_id if we stored that info.
            // Or, rely on timeouts for futures.
        }
    }
}

void RpcClient::onMessageReceived(const std::shared_ptr<vsomeip::message>& msg) {
    // Check if it's a response to one of our requests
    // We need to check client_id and session_id
    // msg->get_client() should be our client_id_
    // msg->get_session() is the session we are waiting for

    if (msg->get_client() == client_id_ &&
        (msg->get_message_type() == vsomeip::message_type_e::MT_RESPONSE ||
         msg->get_message_type() == vsomeip::message_type_e::MT_ERROR)) {

        std::cout << "RpcClient (" << service_name_ << "): Received response/error for session 0x"
                  << std::hex << msg->get_session() << std::dec
                  << ", Type: " << static_cast<int>(msg->get_message_type())
                  << ", RC: " << static_cast<int>(msg->get_return_code()) << std::endl;

        // The generic fulfillPromise will call the stored lambda which knows the ResProto type.
        // This is a simplified way to call the templated fulfill.
        // In a real scenario, the lambda itself stored in PromiseContext would be more complex
        // or we would need a way to get the concrete promise type.
        // For this example, we'll assume `fulfillPromise` can handle it based on the map.
        // This part of the template magic needs careful implementation.
        // The current `fulfillPromise` is not generic enough for this direct call.

        // Corrected approach: The lambda stored in PromiseContext does the work.
        std::lock_guard<std::mutex> lock(pending_requests_mutex_);
        auto key = std::make_pair(client_id_, msg->get_session());
        auto it = pending_requests_.find(key);
        if (it != pending_requests_.end()) {
            it->second.response_parser(msg); // Call the stored lambda
            pending_requests_.erase(it);
        } else {
            // Stale or unexpected response
            std::cout << "RpcClient (" << service_name_ << "): Received response for unknown session 0x"
                      << std::hex << msg->get_session() << std::dec << " for this client." << std::endl;
        }

    } else {
        // Potentially a message not for us or not a response type we handle here.
        // std::cout << "RpcClient (" << service_name_ << "): Received other message type: "
        //           << static_cast<int>(msg->get_message_type()) << std::endl;
    }
}

std::string RpcClient::getServiceName() const {
    return service_name_;
}

bool RpcClient::isServiceAvailable() const {
    return service_available_;
}

} // namespace comms_stack
