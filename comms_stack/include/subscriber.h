#ifndef SUBSCRIBER_H
#define SUBSCRIBER_H

#include <string>
#include <memory>
#include <functional>
#include <set> // For eventgroup set

// Forward declare vsomeip types
namespace vsomeip {
    class application;
    class message;
    using service_t = uint16_t;
    using instance_t = uint16_t;
    // event_t and eventgroup_t are also uint16_t but good for clarity
    using event_t = uint16_t;
    using eventgroup_t = uint16_t;
}
// Forward declare Protobuf message types
namespace google { namespace protobuf { class Message; } }
namespace comms_stack { namespace protos { class SimpleNotification; } }

namespace comms_stack {

class Subscriber {
public:
    using SimpleNotificationCallback = std::function<void(const protos::SimpleNotification& message)>;
    // For generic callback, providing the raw payload might be safer initially,
    // or requiring the user to also provide a parser/prototype.
    // Let's keep it as Message& for now, assuming a mechanism.
    using GenericMessageCallback = std::function<void(const std::string& topic_name, const google::protobuf::Message& message)>;

    Subscriber(const std::string& topic_name,
                 std::shared_ptr<vsomeip::application> app,
                 uint16_t service_id,
                 uint16_t instance_id, // Usually ANY_INSTANCE for subscribers
                 uint16_t event_id_or_group,
                 bool is_eventgroup = true);
    ~Subscriber();

    bool subscribe(SimpleNotificationCallback callback);
    bool subscribeGeneric(GenericMessageCallback callback);
    bool unsubscribe();

    std::string getTopicName() const;
    bool isSubscribed() const;

private:
    void onAvailabilityChanged(vsomeip::service_t service, vsomeip::instance_t instance, bool is_available);
    void onMessageReceived(const std::shared_ptr<vsomeip::message>& msg);

    std::string topic_name_;
    std::shared_ptr<vsomeip::application> vsomeip_app_;
    uint16_t service_id_;
    uint16_t instance_id_; // Instance of the service to monitor for availability
    uint16_t event_id_or_group_;
    bool is_eventgroup_;

    SimpleNotificationCallback notification_callback_;
    GenericMessageCallback generic_callback_;

    bool is_subscribed_ = false;
    bool service_available_ = false; // Track service availability

    // Store the specific eventgroup if it's an eventgroup subscription for request_event
    std::set<vsomeip::eventgroup_t> subscribed_eventgroups_;
};

} // namespace comms_stack

#endif // SUBSCRIBER_H
