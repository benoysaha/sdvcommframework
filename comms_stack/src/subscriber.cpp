#include "subscriber.h"
#include "common_messages.pb.h" // For specific deserialization and GetTypeName()
#include <vsomeip/vsomeip.hpp>
#include <google/protobuf/message.h>
#include <iostream>
#include <vector>

namespace comms_stack {

Subscriber::Subscriber(const std::string& topic_name,
                       std::shared_ptr<vsomeip::application> app,
                       uint16_t service_id,
                       uint16_t instance_id, // Instance to watch for availability
                       uint16_t event_id_or_group,
                       bool is_eventgroup)
    : topic_name_(topic_name),
      vsomeip_app_(app),
      service_id_(service_id),
      instance_id_(instance_id), // Specific instance or vsomeip::ANY_INSTANCE
      event_id_or_group_(event_id_or_group),
      is_eventgroup_(is_eventgroup),
      is_subscribed_(false),
      service_available_(false) {
    if (!vsomeip_app_) {
        std::cerr << "Subscriber (" << topic_name_ << "): vsomeip application is null!" << std::endl;
        return;
    }
    std::cout << "Subscriber: Created for topic: " << topic_name_
              << " (Service: 0x" << std::hex << service_id_
              << ", Instance: 0x" << instance_id_
              << (is_eventgroup_ ? ", Eventgroup: 0x" : ", Event: 0x") << event_id_or_group_
              << std::dec << ")" << std::endl;
    if (is_eventgroup_) {
        subscribed_eventgroups_.insert(event_id_or_group_);
    }
}

Subscriber::~Subscriber() {
    std::cout << "Subscriber: Destroyed for topic: " << topic_name_ << std::endl;
    if (is_subscribed_ && vsomeip_app_) {
        unsubscribe();
    }
}

bool Subscriber::subscribe(SimpleNotificationCallback callback) {
    if (!vsomeip_app_) {
        std::cerr << "Subscriber (" << topic_name_ << "): Cannot subscribe, vsomeip app is null." << std::endl;
        return false;
    }
    if (is_subscribed_) {
        std::cout << "Subscriber (" << topic_name_ << "): Already subscribed." << std::endl;
        // Optionally update callback if different, or just return true
        notification_callback_ = callback;
        generic_callback_ = nullptr;
        return true;
    }

    notification_callback_ = callback;
    generic_callback_ = nullptr;

    // Register availability handler for the service instance we care about
    vsomeip_app_->register_availability_handler(
        service_id_,
        instance_id_, // Watch specific instance or vsomeip::ANY_INSTANCE
        std::bind(&Subscriber::onAvailabilityChanged, this,
                  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
    );
    std::cout << "Subscriber (" << topic_name_ << "): Registered availability handler for Service 0x"
              << std::hex << service_id_ << ", Instance 0x" << instance_id_ << std::dec << std::endl;

    // Register message handler for the event or events in the eventgroup
    vsomeip_app_->register_message_handler(
        service_id_,
        instance_id_, // Or vsomeip::ANY_INSTANCE if messages can come from any provider instance
        event_id_or_group_, // If is_eventgroup_ is true, this is the eventgroup ID.
                            // vsomeip handles routing messages for any event within this group.
                            // If false, this is a specific event ID.
        std::bind(&Subscriber::onMessageReceived, this, std::placeholders::_1)
    );
    std::cout << "Subscriber (" << topic_name_ << "): Registered message handler for "
              << (is_eventgroup_ ? "Eventgroup 0x" : "Event 0x") << std::hex << event_id_or_group_
              << std::dec << std::endl;

    // Request the event or eventgroup
    // If it's an eventgroup, subscribed_eventgroups_ contains its ID.
    // If it's a specific event, subscribed_eventgroups_ can be empty or specific if the event belongs to a group.
    // For a specific event not in a group, the set is typically empty.
    if (is_eventgroup_) {
         vsomeip_app_->request_event(
            service_id_,
            instance_id_, // Instance providing the event group, or ANY_INSTANCE
            event_id_or_group_, // This is treated as a "major event ID" by someip, often 0xFFFF for event groups
                               // but vsomeip uses the actual eventgroup ID here for the _event parameter
                               // and the set for _eventgroups. It's a bit confusing.
                               // Let's assume event_id_or_group_ is the event ID to request (even if it's also group ID)
                               // and subscribed_eventgroups_ correctly lists the group.
            subscribed_eventgroups_,
            vsomeip::event_type_e::ET_EVENT); // Or ET_FIELD if applicable
    } else {
        // Requesting a specific event
         vsomeip_app_->request_event(
            service_id_,
            instance_id_,
            event_id_or_group_, // The specific event ID
            {}, // No specific eventgroup set, or the relevant one if known
            vsomeip::event_type_e::ET_EVENT);
    }

    std::cout << "Subscriber (" << topic_name_ << "): Requested "
              << (is_eventgroup_ ? "eventgroup 0x" : "event 0x") << std::hex << event_id_or_group_
              << std::dec << std::endl;

    is_subscribed_ = true;
    return true;
}

bool Subscriber::subscribeGeneric(GenericMessageCallback callback) {
    // For now, this is largely the same as the specific subscribe.
    // The main difference is in onMessageReceived for deserialization.
    if (!vsomeip_app_) {
        std::cerr << "Subscriber (" << topic_name_ << "): Cannot subscribe, vsomeip app is null." << std::endl;
        return false;
    }
    if (is_subscribed_) {
        std::cout << "Subscriber (" << topic_name_ << "): Already subscribed (generic)." << std::endl;
        generic_callback_ = callback;
        notification_callback_ = nullptr;
        return true;
    }

    generic_callback_ = callback;
    notification_callback_ = nullptr;

    vsomeip_app_->register_availability_handler(
        service_id_, instance_id_,
        std::bind(&Subscriber::onAvailabilityChanged, this,
                  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
     std::cout << "Subscriber (" << topic_name_ << "): Registered availability handler (generic) for Service 0x"
              << std::hex << service_id_ << ", Instance 0x" << instance_id_ << std::dec << std::endl;


    vsomeip_app_->register_message_handler(
        service_id_, instance_id_, event_id_or_group_,
        std::bind(&Subscriber::onMessageReceived, this, std::placeholders::_1));
    std::cout << "Subscriber (" << topic_name_ << "): Registered message handler (generic) for "
              << (is_eventgroup_ ? "Eventgroup 0x" : "Event 0x") << std::hex << event_id_or_group_
              << std::dec << std::endl;

    if (is_eventgroup_) {
        vsomeip_app_->request_event(service_id_, instance_id_, event_id_or_group_, subscribed_eventgroups_, vsomeip::event_type_e::ET_EVENT);
    } else {
        vsomeip_app_->request_event(service_id_, instance_id_, event_id_or_group_, {}, vsomeip::event_type_e::ET_EVENT);
    }
    std::cout << "Subscriber (" << topic_name_ << "): Requested (generic) "
              << (is_eventgroup_ ? "eventgroup 0x" : "event 0x") << std::hex << event_id_or_group_
              << std::dec << std::endl;

    is_subscribed_ = true;
    return true;
}

bool Subscriber::unsubscribe() {
    if (!vsomeip_app_) {
         std::cerr << "Subscriber (" << topic_name_ << "): Cannot unsubscribe, vsomeip app is null." << std::endl;
        return !is_subscribed_; // Return true if not subscribed, false if was subscribed but app is null
    }
    if (!is_subscribed_) {
        std::cout << "Subscriber (" << topic_name_ << "): Not currently subscribed." << std::endl;
        return true;
    }

    // Unregister message handler
    vsomeip_app_->unregister_message_handler(
        service_id_, instance_id_, event_id_or_group_);

    // Release the event/eventgroup
    if (is_eventgroup_) {
        vsomeip_app_->release_event(service_id_, instance_id_, event_id_or_group_, subscribed_eventgroups_);
    } else {
        vsomeip_app_->release_event(service_id_, instance_id_, event_id_or_group_, {});
    }

    // Unregister availability handler
    vsomeip_app_->unregister_availability_handler(
        service_id_, instance_id_,
        std::bind(&Subscriber::onAvailabilityChanged, this,
                  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    // Note: Unregistering the *exact same* std::function object can be tricky if the std::bind creates a new one.
    // vsomeip might handle this by service/instance pair, or it might require storing the bound functor.
    // For simplicity, we assume vsomeip unregisters based on service/instance. If not, a more robust
    // handler storage and unregistration mechanism is needed (e.g., using vsomeip:: γίνεται_handler_type_t).
    // A common practice is to just unregister all handlers for a service/instance if one is unregistering.
    // Or, more simply, vsomeip might allow unregistering with just service_id and instance_id.
    // Let's assume the unregister call with a new bind works or it unregisters all for that service/instance.
    // A safer way for availability is to use register_availability_handler(service, instance, handler, connection_id)
    // and then unregister_availability_handler(connection_id). But this requires managing connection_id.
    // For now, we'll use the simpler unregister form.

    std::cout << "Subscriber (" << topic_name_ << "): Unsubscribed from "
              << (is_eventgroup_ ? "eventgroup 0x" : "event 0x") << std::hex << event_id_or_group_
              << std::dec << std::endl;

    notification_callback_ = nullptr;
    generic_callback_ = nullptr;
    is_subscribed_ = false;
    service_available_ = false;
    return true;
}

void Subscriber::onAvailabilityChanged(vsomeip::service_t service, vsomeip::instance_t instance, bool is_available) {
    if (service == service_id_ && instance == instance_id_) { // Check if it's for the service we are interested in
        service_available_ = is_available;
        std::cout << "Subscriber (" << topic_name_ << "): Availability changed for Service 0x"
                  << std::hex << service << ", Instance 0x" << instance
                  << " -> " << (is_available ? "AVAILABLE" : "NOT AVAILABLE") << std::dec << std::endl;

        if (is_available && is_subscribed_) {
            // Service became available, ensure our event request is active
            // (vsomeip usually handles re-requesting if service appears after initial request)
            // We can re-issue request_event here if necessary, but often not needed.
            std::cout << "Subscriber (" << topic_name_ << "): Service is now available. Event subscription should be active." << std::endl;
        } else if (!is_available) {
            std::cout << "Subscriber (" << topic_name_ << "): Service is no longer available." << std::endl;
        }
    }
}

void Subscriber::onMessageReceived(const std::shared_ptr<vsomeip::message>& msg) {
    // Check if the message is for the event/eventgroup we are interested in.
    // This check is important if a single handler is registered for ANY_EVENT or if an eventgroup
    // handler receives events from multiple specific events within that group.
    // If we registered for a specific event_id_or_group_, this check is more of a safeguard.
    if (msg->get_service() == service_id_ &&
        (is_eventgroup_ || msg->get_event() == event_id_or_group_)) {
        // If it's an eventgroup, the event ID in the message (msg->get_event()) will be the specific event
        // from that group that was triggered. event_id_or_group_ is our group ID.
        // We need to ensure the message is indeed for an event we care about if subscribed to a group.
        // For now, if is_eventgroup_ is true, we assume any message for that service on ANY_EVENT (if handler was so general)
        // or the group ID (if handler was for group) is relevant.

        std::shared_ptr<vsomeip::payload> payload = msg->get_payload();
        if (!payload || payload->get_length() == 0) {
            std::cerr << "Subscriber (" << topic_name_ << "): Received empty payload for event 0x"
                      << std::hex << msg->get_event() << std::dec << std::endl;
            return;
        }

        const vsomeip::byte_t* data = payload->get_data();
        vsomeip::length_t len = payload->get_length();

        std::cout << "Subscriber (" << topic_name_ << "): Message received for event 0x"
                  << std::hex << msg->get_event() << std::dec << " (Payload size: " << len << ")" << std::endl;

        if (notification_callback_) {
            protos::SimpleNotification notification;
            if (notification.ParseFromArray(data, len)) {
                notification_callback_(notification);
            } else {
                std::cerr << "Subscriber (" << topic_name_ << "): Failed to parse SimpleNotification." << std::endl;
            }
        } else if (generic_callback_) {
            // For generic callback, we need a way to know WHAT message type to parse into.
            // This is a complex problem. A common solution is to have a factory or a map
            // from an identifier (e.g., event ID, or a type field within the message itself)
            // to a std::function that can create and parse the correct message type.
            // For now, we'll indicate this limitation.
            std::cerr << "Subscriber (" << topic_name_
                      << "): Generic callback invoked, but dynamic Protobuf message parsing not fully implemented. "
                      << "Payload received, but cannot determine specific type." << std::endl;
            // One simple approach IF the type is known by topic_name (e.g. only one type per topic):
            // if (topic_name_ == "some_known_topic_for_simple_notification") {
            //    protos::SimpleNotification concrete_message;
            //    if (concrete_message.ParseFromArray(data, len)) {
            //       generic_callback_(topic_name_, concrete_message);
            //    } // ...
            // }
        }
    } else {
         // Message for a different service/event, ignore if our handler was too broad.
    }
}

std::string Subscriber::getTopicName() const {
    return topic_name_;
}

bool Subscriber::isSubscribed() const {
    return is_subscribed_;
}

} // namespace comms_stack
