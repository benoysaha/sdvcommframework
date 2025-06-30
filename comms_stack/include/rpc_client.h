#ifndef RPC_CLIENT_H
#define RPC_CLIENT_H

#include <string>
#include <memory>
#include <functional>
#include <future>
#include <map> // For pending requests
#include <mutex> // For pending_requests_mutex_

// Forward declare vsomeip types
namespace vsomeip {
    class application;
    class message;
    class client_t; // For client ID
    using service_t = uint16_t;
    using instance_t = uint16_t;
    using method_t = uint16_t;
    using session_t = uint16_t;
}
// Forward declare Protobuf message types
namespace comms_stack { namespace protos {
    class EchoRequest;
    class EchoResponse;
    class AddRequest;
    class AddResponse;
}}
// Forward declare google::protobuf::Message for generic response parsing (if needed)
namespace google { namespace protobuf { class Message; } }


namespace comms_stack {

class RpcClient {
public:
    RpcClient(const std::string& service_name,
              std::shared_ptr<vsomeip::application> app,
              uint16_t service_id,
              uint16_t instance_id); // Target service instance
    ~RpcClient();

    std::future<protos::EchoResponse> Echo(const protos::EchoRequest& request);
    std::future<protos::AddResponse> Add(const protos::AddRequest& request);

    std::string getServiceName() const;
    bool isServiceAvailable() const;

private:
    void onAvailabilityChanged(vsomeip::service_t service, vsomeip::instance_t instance, bool is_available);
    void onMessageReceived(const std::shared_ptr<vsomeip::message>& msg);

    // Helper to send request and manage promise
    template<typename ResProto>
    void registerPromise(vsomeip::client_t client_id, vsomeip::session_t session_id, std::promise<ResProto> promise);

    template<typename ResProto>
    void fulfillPromise(vsomeip::client_t client_id, vsomeip::session_t session_id, const std::shared_ptr<vsomeip::message>& msg);

    void fulfillPromiseWithError(vsomeip::client_t client_id, vsomeip::session_t session_id, const std::string& error_msg);


    std::string service_name_;
    std::shared_ptr<vsomeip::application> vsomeip_app_;
    uint16_t service_id_;
    uint16_t instance_id_; // Target service instance ID
    vsomeip::client_t client_id_; // Our own client ID, assigned by vsomeip

    bool service_available_ = false;

    // For managing asynchronous responses
    struct PromiseContext {
        std::function<void(const std::shared_ptr<vsomeip::message>&)> response_parser;
    };
    std::map<std::pair<vsomeip::client_t, vsomeip::session_t>, PromiseContext> pending_requests_;
    std::mutex pending_requests_mutex_; // Protect access to pending_requests_
};

} // namespace comms_stack

#endif // RPC_CLIENT_H
