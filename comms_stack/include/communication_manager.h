#ifndef COMMUNICATION_MANAGER_H
#define COMMUNICATION_MANAGER_H

#include <string>
#include <memory>
#include <functional>
#include <map> // For caches
#include "sample_rpc_service.pb.h" // Include the generated service header


// vsomeip forward declaration (or include if small)
namespace vsomeip {
    class application; // Forward declare
    // If using version 3.x (likely)
    namespace cfg {
        // No specific forward declaration usually needed for basic init
    }
}

// Forward declarations for our classes
namespace comms_stack {
    class Publisher;
    class Subscriber;
    // class RpcService; // Using protos::SampleRpc directly for now
    class RpcClient;
    namespace protos {
        class SimpleNotification;
        // SampleRpc is now included directly
    }
}

namespace comms_stack {

class CommunicationManager {
public:
    static CommunicationManager& getInstance();

    CommunicationManager(const CommunicationManager&) = delete;
    CommunicationManager& operator=(const CommunicationManager&) = delete;

    bool init(const std::string& app_name = "CommsStackApp", const std::string& config_path = ""); // config_path can be optional
    void shutdown();

    std::shared_ptr<Publisher> getPublisher(const std::string& topic_name);
    std::shared_ptr<Subscriber> getSubscriber(const std::string& topic_name);
    // Changed RpcService to the specific generated type protos::SampleRpc
    void registerRpcService(const std::string& user_service_name,
                              uint16_t service_id, uint16_t instance_id, // These would come from config
                              std::shared_ptr<protos::SampleRpc> service_impl);
    std::shared_ptr<RpcClient> getRpcClient(const std::string& service_name);

    // Expose vsomeip application for internal use by Publisher/Subscriber/etc.
    std::shared_ptr<vsomeip::application> getVsomeipApplication();

private:
    CommunicationManager();
    ~CommunicationManager();

    bool is_initialized_ = false;
    std::string app_name_;
    std::shared_ptr<vsomeip::application> vsomeip_app_;

    // Caches
    std::map<std::string, std::shared_ptr<Publisher>> publisher_cache_;
    std::map<std::string, std::shared_ptr<Subscriber>> subscriber_cache_;
    std::map<std::string, std::shared_ptr<RpcClient>> rpc_client_cache_;
    // Store the specific service implementations
    // Key: user_service_name or internal service_id
    std::map<std::string, std::shared_ptr<protos::SampleRpc>> actual_rpc_services_;
    // We might also need to store registered method handlers if they are member functions
    // or need to be explicitly unregistered. For lambdas, vsomeip handles it.

    // Thread for vsomeip processing (alternative to vsomeip_app_->start() if more control is needed)
    // std::unique_ptr<std::thread> processing_thread_;
    // bool running_ = false;
    // void processMessages(); // If using custom thread
};

} // namespace comms_stack

#endif // COMMUNICATION_MANAGER_H
