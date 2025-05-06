#ifndef GATEWAY_COMPONENT_H
#define GATEWAY_COMPONENT_H

#include <chrono>
#include <string>

#include "component.h"
#include "debug.h"

class GatewayComponent : public Component {
    public:
        static const unsigned int PORT;

        GatewayComponent(Vehicle* vehicle, const unsigned int vehicle_id, 
                         const std::string& name, VehicleProt* protocol);

        // Use default destructor instead of empty implementation
        ~GatewayComponent() = default;

        void run() override;
        
    private:
        // Helper method to process and log received messages
        void process_message(const Message& message, const std::chrono::microseconds& recv_time);
};

/******** Gateway Component Implementation *******/
const unsigned int GatewayComponent::PORT = static_cast<unsigned int>(Vehicle::Ports::BROADCAST);

GatewayComponent::GatewayComponent(Vehicle* vehicle, const unsigned int vehicle_id, 
                                  const std::string& name, VehicleProt* protocol) 
    : Component(vehicle, vehicle_id, name) 
{
    // Sets CSV result Header
    open_log_file();
    if (_log_file.is_open()) {
        _log_file.seekp(0); // Go to beginning to overwrite if file exists
        // Define log header
        _log_file << "receive_timestamp_us,source_address,source_component_type,source_vehicle,"
                 << "message_id,event_type,send_timestamp_us,latency_us,raw_message\n";
        _log_file.flush();
    }

    // Sets own address
    Address addr(_vehicle->address(), PORT);

    // Sets own communicator
    _communicator = new Comms(protocol, addr);
}

void GatewayComponent::run() {
    db<GatewayComponent>(INF) << "[GatewayComponent] thread running.\n";

    while (running()) {
        // Waits for message
        Message message = _communicator->new_message(Message::Type::RESPONSE, 0);
        
        // Receive message directly into message object
        bool received = _communicator->receive(&message);

        if (received) {
            auto recv_time = std::chrono::system_clock::now();
            auto recv_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
                recv_time.time_since_epoch());
            
            process_message(message, recv_time_us);
        }
    }
    
    db<GatewayComponent>(INF) << "[GatewayComponent] thread terminated.\n";
}

void GatewayComponent::process_message(const Message& message, 
                                      const std::chrono::microseconds& recv_time) {
    // Log message reception
    db<GatewayComponent>(INF) << "[GatewayComponent] Received message from " 
                             << message.origin().to_string() << "\n";
    
    // Log to CSV if file is open
    if (_log_file.is_open()) {
        try {
            _log_file << recv_time.count() << ","
                     << message.origin().to_string() << ","
                     << "unknown" << ","  // component type
                     << "unknown" << ","  // source vehicle
                     << message.unit_type() << ","
                     << "receive" << ","
                     << message.timestamp() << ","
                     << (recv_time.count() - message.timestamp()) << ","
                     << "raw_message_placeholder" << "\n";
            _log_file.flush();
        } catch (const std::exception& e) {
            db<GatewayComponent>(ERR) << "[GatewayComponent] error writing to log file: " 
                                     << e.what() << "\n";
        }
    }
}

#endif // GATEWAY_COMPONENT_H