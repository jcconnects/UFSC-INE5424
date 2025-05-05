#include "component.h"
#include "debug.h"

class GatewayComponent : public Component {
    public:
        static const unsigned int PORT;

        GatewayComponent(Vehicle* vehicle, const unsigned int vehicle_id, const std::string& name, VehicleProt* protocol);

        ~GatewayComponent();

        void run() override;
};

/******** Gateway Component Implementation *******/
const unsigned int GatewayComponent::PORT = static_cast<unsigned int>(Vehicle::Ports::BROADCAST);

GatewayComponent::GatewayComponent(Vehicle* vehicle, const unsigned int vehicle_id, const std::string& name, VehicleProt* protocol) : Component(vehicle, vehicle_id, name) {
    // Sets CSV result Header
    open_log_file();
    if (_log_file.is_open()) {
        _log_file.seekp(0); // Go to beginning to overwrite if file exists
        // Define log header
        _log_file << "receive_timestamp_us,source_address,source_component_type,source_vehicle,message_id,event_type,send_timestamp_us,latency_us,raw_message\n";
        _log_file.flush();
    }

    // Sets own address
    Address addr(_vehicle->address(), PORT);

    // Sets own communicator
    _communicator = new Comms(protocol, addr);
}

void GatewayComponent::run() {
    db<GatewayComponent>(INF) << "[GatewayComponent] thread running.\n";

    unsigned int counter = 0;

    while (running()) {
        // Waits for message
        
    }
}