#ifndef GATEWAY_COMPONENT_H
#define GATEWAY_COMPONENT_H

#include <chrono>
#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <algorithm>

#include "component.h"
#include "debug.h"
#include "teds.h"
#include "message.h"
#include "vehicle.h"

class GatewayComponent : public Component {
    public:
        typedef std::uint16_t Port;
        static const unsigned int PORT;

        GatewayComponent(Vehicle* vehicle, const unsigned int vehicle_id, 
                         const std::string& name, VehicleProt* protocol);

        // Use default destructor instead of empty implementation
        ~GatewayComponent() = default;

        void run() override;
        
    protected:
        // Hide component_dispatcher_routine to directly handle Gateway-specific messages
        void component_dispatcher_routine() override;
        
    private:
        // Helper methods to handle specific message types
        void handle_interest(const Message& message);
        void handle_response(const Message& message);
        
        // REMOVED: Get producer port for a given data type using vehicle's hardcoded mappings
        // std::uint16_t get_producer_port(DataTypeId type) const; 
        
        // REMOVED: Get consumer ports interested in a given data type
        // std::vector<std::uint16_t> get_consumer_ports(DataTypeId type) const;
        
        // REMOVED: Map to track which data types are requested by which consumer ports
        // std::map<DataTypeId, std::vector<std::uint16_t>> _consumer_interests;
};

/******** Gateway Component Implementation *******/
const unsigned int GatewayComponent::PORT = 0; // Gateway always uses Port 0

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

    // Sets own address using Port 0 (Gateway's well-known port)
    Address addr(_vehicle->address(), PORT);

    // Sets own communicator
    _communicator = new Comms(protocol, addr, ComponentType::GATEWAY, DataTypeId::OBSTACLE_DISTANCE);

    db<GatewayComponent>(INF) << "[Gateway] " << getName() << " initialized on port " << PORT << "\n";
}

void GatewayComponent::run() {
    db<GatewayComponent>(INF) << "[Gateway] " << getName() << " starting main run loop.\n";
    
    // The main Gateway logic is handled in component_dispatcher_routine override
    while (running()) {
        // This thread just stays alive and periodically logs status if needed
        usleep(5000000); // Sleep for 5 seconds
        
        db<GatewayComponent>(INF) << "[Gateway] " << getName() << " still running.\n";
    }
    
    db<GatewayComponent>(INF) << "[Gateway] " << getName() << " exiting main run loop.\n";
}

void GatewayComponent::component_dispatcher_routine() {
    db<GatewayComponent>(TRC) << "[Gateway] " << getName() << " dispatcher routine started.\n";
    
    // Buffer for raw messages
    // std::uint8_t raw_buffer[1024]; // Not used if receive directly populates Message
    
    while (_dispatcher_running.load()) {
        Message message = _communicator->new_message(Message::Type::RESPONSE, DataTypeId::UNKNOWN); // Default message
        int recv_size = receive(&message); // receive is a Component method
        
        if (recv_size <= 0) {
            // Check if we should exit
            if (!_dispatcher_running.load()) {
                break;
            }
            
            // Handle error or timeout
            if (recv_size < 0) {
                db<GatewayComponent>(ERR) << "[Gateway] " << getName() << " dispatcher receive error: " << recv_size << "\n";
            }
            
            // Continue to next iteration
            continue;
        }
        
        try {
            // Get current time for latency measurement
            auto recv_time = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch());
        
            
            // Process the message directly based on its type
            if (message.message_type() == Message::Type::INTEREST) {
                handle_interest(message);
            }
            else if (message.message_type() == Message::Type::RESPONSE) {
                handle_response(message);
            }
            else {
                db<GatewayComponent>(WRN) << "Gateway received unhandled message type: " 
                                         << static_cast<int>(message.message_type()) << "\n";
            }
            
            // Log the message receipt
            if (_log_file.is_open()) {
                auto latency_us = recv_time.count() - message.timestamp();
                _log_file << recv_time.count() << ","
                         << message.origin().to_string() << ","
                         << "UNKNOWN" << "," // Component type not tracked currently
                         << message.origin() << ","
                         << message.timestamp() << ","
                         << static_cast<int>(message.message_type()) << ","
                         << message.timestamp() << ","
                         << latency_us << ","
                         << "RAW_NOT_IMPLEMENTED" << "\n";
                _log_file.flush();
            }
            
        } catch (const std::exception& e) {
            db<GatewayComponent>(ERR) << "[Gateway] " << getName() << " dispatcher exception: " << e.what() << "\n";
        }
    }
    
    db<GatewayComponent>(TRC) << "[Gateway] " << getName() << " dispatcher routine exiting.\n";
}

void GatewayComponent::handle_interest(const Message& message) {
    DataTypeId requested_type = message.unit_type();
    std::uint32_t period = message.period();
    Address original_requester = message.origin();

    db<GatewayComponent>(INF) << "[Gateway] handle_interest: Received INTEREST for type " 
                             << static_cast<int>(requested_type) << " with period " << period 
                             << " from original requester: " << original_requester.to_string() 
                             << ". Relaying as broadcast.\n";

    // Simply forward the message as a broadcast - Protocol layer will handle delivery
    // to all interested components
    Address broadcast_addr(Ethernet::Address::BROADCAST, 0); // Port 0 is broadcast
    _communicator->send(message, broadcast_addr);
    
    db<GatewayComponent>(INF) << "[Gateway] INTEREST relayed via Protocol layer broadcast\n";
}

void GatewayComponent::handle_response(const Message& message) {
    DataTypeId response_type = message.unit_type();
    Address original_producer = message.origin();
    
    db<GatewayComponent>(INF) << "[Gateway] handle_response: Received RESPONSE for data type " 
                             << static_cast<int>(response_type) 
                             << " from " << original_producer.to_string() 
                             << ". Relaying as broadcast.\n";

    // Simply forward the message as a broadcast - Protocol layer will handle delivery
    // to all interested components
    Address broadcast_addr(Ethernet::Address::BROADCAST, 0); // Port 0 is broadcast
    _communicator->send(message, broadcast_addr);
    
    db<GatewayComponent>(INF) << "[Gateway] RESPONSE relayed via Protocol layer broadcast\n";
}

#endif // GATEWAY_COMPONENT_H