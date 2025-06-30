#ifndef PUBLISHER_H
#define PUBLISHER_H

#include <string>
#include <memory>

// Forward declare vsomeip application
namespace vsomeip { class application; }
// Forward declare Protobuf message types
namespace google { namespace protobuf { class Message; } }
namespace comms_stack { namespace protos { class SimpleNotification; } }


namespace comms_stack {

class Publisher {
public:
    Publisher(const std::string& topic_name,
              std::shared_ptr<vsomeip::application> app,
              uint16_t service_id, // For now, pass IDs directly
              uint16_t instance_id,
              uint16_t eventgroup_id_or_event_id,
              bool is_eventgroup = true); // True if eventgroup_id_or_event_id is an eventgroup
    ~Publisher();

    bool publish(const protos::SimpleNotification& message);
    bool publishGeneric(const google::protobuf::Message& message);

    std::string getTopicName() const;
    bool isOffered() const;

private:
    std::string topic_name_;
    std::shared_ptr<vsomeip::application> vsomeip_app_;
    uint16_t service_id_;
    uint16_t instance_id_;
    uint16_t event_id_or_group_; // Can be event ID or eventgroup ID
    bool is_eventgroup_;
    bool is_offered_ = false;

    void offer(); // Helper to offer event
};

} // namespace comms_stack

#endif // PUBLISHER_H
