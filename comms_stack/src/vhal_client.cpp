#include "subscriber.h"
#include "vhal.pb.h"
#include <iostream>

void onVhalPropertyUpdate(const vhal::VhalPropertyUpdate& update) {
    const auto& prop = update.property();
    std::cout << "Received VHAL property update:" << std::endl;
    std::cout << "  ID: " << prop.id() << std::endl;
    std::cout << "  Area ID: " << prop.area_id() << std::endl;

    switch (prop.value_case()) {
        case vhal::VhalProperty::kBoolValue:
            std::cout << "  Value: " << prop.bool_value() << std::endl;
            break;
        case vhal::VhalProperty::kFloatValue:
            std::cout << "  Value: " << prop.float_value() << std::endl;
            break;
        case vhal::VhalProperty::kInt32Value:
            std::cout << "  Value: " << prop.int32_value() << std::endl;
            break;
        case vhal::VhalProperty::kStringValue:
            std::cout << "  Value: " << prop.string_value() << std::endl;
            break;
        case vhal::VhalProperty::kBytesValue:
            std::cout << "  Value: <bytes>" << std::endl;
            break;
        case vhal::VhalProperty::kInt64Value:
            std::cout << "  Value: " << prop.int64_value() << std::endl;
            break;
        default:
            std::cout << "  Value: <unknown>" << std::endl;
            break;
    }
}

int main(int argc, char* argv[]) {
    // This is a conceptual client. In a real application, this would be
    // integrated into the main application logic.
    // The CommunicationManager would need to be initialized, and the
    // vsomeip application would need to be started.

    // For demonstration purposes, we assume the CommunicationManager is initialized
    // and we can get a subscriber.

    // auto& comm_mgr = comms_stack::CommunicationManager::getInstance();
    // comm_mgr.init("vhal_client", "path/to/config.json");
    // auto vsomeip_app = comm_mgr.getVsomeipApplication();

    // const std::string topicName = "vhal_property_updates";
    // const uint16_t service_id = 0x3333;
    // const uint16_t instance_id = 0x0001;
    // const uint16_t eventgroup_id = 0x9200;

    // comms_stack::Subscriber subscriber(topicName, vsomeip_app, service_id, instance_id, eventgroup_id, true);
    // subscriber.subscribe(onVhalPropertyUpdate);

    // std::cout << "VHAL client subscribed to property updates." << std::endl;

    // In a real application, you would have a loop here to keep the client running.
    // while (true) {
    //     std::this_thread::sleep_for(std::chrono::seconds(1));
    // }

    return 0;
}
