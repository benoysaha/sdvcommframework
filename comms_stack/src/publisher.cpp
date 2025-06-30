#include "publisher.h"
#include "common_messages.pb.h" // For specific publish method, and GetTypeName()
#include <vsomeip/vsomeip.hpp>
#include <google/protobuf/message.h>
#include <iostream>
#include <vector> // For payload data
#include <set> // For eventgroup set in offer_event

namespace comms_stack {

Publisher::Publisher(const std::string& topic_name,
                     std::shared_ptr<vsomeip::application> app,
                     uint16_t service_id,
                     uint16_t instance_id,
                     uint16_t eventgroup_id_or_event_id,
                     bool is_eventgroup)
    : topic_name_(topic_name),
      vsomeip_app_(app),
      service_id_(service_id),
      instance_id_(instance_id),
      event_id_or_group_(eventgroup_id_or_event_id),
      is_eventgroup_(is_eventgroup),
      is_offered_(false) {
    if (!vsomeip_app_) {
        std::cerr << "Publisher (" << topic_name_ << "): vsomeip application is null!" << std::endl;
        return;
    }
    std::cout << "Publisher: Created for topic: " << topic_name_
              << " (Service: 0x" << std::hex << service_id_
              << ", Instance: 0x" << instance_id_
              << (is_eventgroup_ ? ", Eventgroup: 0x" : ", Event: 0x") << event_id_or_group_
              << std::dec << ")" << std::endl;
    offer(); // Call offer helper
}

Publisher::~Publisher() {
    std::cout << "Publisher: Destroyed for topic: " << topic_name_ << std::endl;
    if (is_offered_ && vsomeip_app_) {
        // For event groups, and also for specific events that were offered.
        vsomeip_app_->stop_offer_event(service_id_, instance_id_, event_id_or_group_);
        std::cout << "Publisher (" << topic_name_ << "): Stopped offering "
                  << (is_eventgroup_ ? "eventgroup 0x" : "event 0x")
                  << std::hex << event_id_or_group_ << std::dec << std::endl;
        is_offered_ = false;
    }
}

void Publisher::offer() {
    if (!vsomeip_app_) {
        std::cerr << "Publisher (" << topic_name_ << "): Cannot offer, vsomeip application is null." << std::endl;
        return;
    }
    if (is_offered_) {
        std::cout << "Publisher (" << topic_name_ << "): Already offered." << std::endl;
        return;
    }

    std::set<vsomeip::eventgroup_t> event_groups;
    if(is_eventgroup_) {
        event_groups.insert(event_id_or_group_);
    }
    // If it's a specific event not part of a group, the set can be empty or contain 0,
    // depending on vsomeip interpretation or specific needs.
    // For simplicity, if it's not an eventgroup, we offer the event without specifying a group in the set.
    // The event_id_or_group_ is the event_id in this case.

    vsomeip_app_->offer_event(
        service_id_,
        instance_id_,
        event_id_or_group_,
        event_groups,
        vsomeip::event_type_e::ET_EVENT);

    is_offered_ = true;
    std::cout << "Publisher (" << topic_name_ << "): Offered "
              << (is_eventgroup_ ? "eventgroup 0x" : "event 0x") << std::hex << event_id_or_group_
              << std::dec << " for Service 0x" << std::hex << service_id_ << std::dec << std::endl;
}


bool Publisher::publish(const protos::SimpleNotification& message) {
    return publishGeneric(message);
}

bool Publisher::publishGeneric(const google::protobuf::Message& message) {
    if (!vsomeip_app_) {
        std::cerr << "Publisher (" << topic_name_ << "): Cannot publish, vsomeip application is null." << std::endl;
        return false;
    }
    if (!is_offered_) {
        std::cout << "Publisher (" << topic_name_ << "): Event/group not offered. Attempting to offer now." << std::endl;
        offer();
        if(!is_offered_){
            std::cerr << "Publisher (" << topic_name_ << "): Failed to offer event/group, cannot publish." << std::endl;
            return false;
        }
    }

    std::string serialized_data;
    if (!message.SerializeToString(&serialized_data)) {
        std::cerr << "Publisher (" << topic_name_ << "): Failed to serialize " << message.GetTypeName() << std::endl;
        return false;
    }

    std::shared_ptr<vsomeip::payload> payload = vsomeip::runtime::get()->create_payload();
    // Use a std::vector<vsomeip::byte_t> for payload to ensure lifetime if needed, though for set_data it copies.
    std::vector<vsomeip::byte_t> payload_data(serialized_data.begin(), serialized_data.end());
    payload->set_data(payload_data);


    if (is_eventgroup_) {
         vsomeip_app_->fire_event(
            service_id_,
            instance_id_,
            event_id_or_group_, // This is the eventgroup ID
            payload,
            vsomeip::reliable_e::UNRELIABLE); // Or vsomeip::reliable_e::RELIABLE based on topic config
    } else {
        vsomeip_app_->notify(
            service_id_,
            instance_id_,
            event_id_or_group_, // This is the specific event ID
            payload,
            vsomeip::reliable_e::UNRELIABLE); // Or vsomeip::reliable_e::RELIABLE
    }


    std::cout << "Publisher (" << topic_name_ << "): Published " << message.GetTypeName()
              << " (size: " << serialized_data.length() << " bytes) to "
              << (is_eventgroup_ ? "eventgroup 0x" : "event 0x") << std::hex << event_id_or_group_
              << std::dec << std::endl;
    return true;
}

std::string Publisher::getTopicName() const {
    return topic_name_;
}

bool Publisher::isOffered() const {
    return is_offered_;
}

} // namespace comms_stack
