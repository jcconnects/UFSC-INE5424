#ifndef GATEWAY_COMPONENT_H
#define GATEWAY_COMPONENT_H

#include <chrono>
#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <algorithm>

// Include only the necessary headers and forward declare the rest
#include "teds.h"
#include "message.h"
#include "debug.h"
#include "ethernet.h"

// Forward declarations
class Vehicle;
class SocketEngine;
class SharedMemoryEngine;

template <typename Engine1, typename Engine2>
class NIC;

template <typename NIC>
class Protocol;

class Component;

class GatewayComponent : public Component {
    public:
        static const unsigned int PORT; // Gateway always uses Port 0

        GatewayComponent(Vehicle* vehicle, const unsigned int vehicle_id, 
                         const std::string& name, Protocol<NIC<SocketEngine, SharedMemoryEngine>>* protocol);

        // Use default destructor instead of empty implementation
        ~GatewayComponent() = default;

        void run() override;
        
    protected:
        // Removed: void component_dispatcher_routine() override;
        
    private:
        // Removed: void handle_interest(const Message& message);
        // Removed: void handle_response(const Message& message);
};

// Move implementation to a separate .cpp file
#include "component.h"  // Now can safely include Component
#include "vehicle.h"    // For _vehicle->protocol()

// Implement the constructor
GatewayComponent::GatewayComponent(Vehicle* vehicle, const unsigned int vehicle_id, 
                                  const std::string& name, Protocol<NIC<SocketEngine, SharedMemoryEngine>>* protocol) 
    : Component(vehicle, vehicle_id, name, ComponentType::GATEWAY) 
{
    db<GatewayComponent>(TRC) << "[Gateway] constructor called.\n";
    // Sets CSV result Header
    open_log_file();
    if (_log_file.is_open()) {
        _log_file.seekp(0); // Go to beginning to overwrite if file exists
        // Define log header
        _log_file << "receive_timestamp_us,source_address,source_component_type,source_vehicle,"
                 << "message_id,event_type,send_timestamp_us,latency_us,raw_message\n";
        _log_file.flush();
    }

    db<GatewayComponent>(INF) << "[Gateway] Log created.\n";

    // Sets own address using Port 0 (Gateway's well-known port)
    // The Component base class constructor now uses GATEWAY_PORT for the gateway address
    // and the component's specific port is set up in the communicator.
    // The Gateway's specific port for its communicator should be GATEWAY_PORT (0).
    Address addr(_vehicle->address(), GATEWAY_PORT); // Gateway's own address is on port 0.

    db<GatewayComponent>(INF) << "[Gateway] Address set to " << addr.to_string() << "\n";

    // Sets own communicator. For Gateway, it listens on GATEWAY_PORT (0).
    // The DataTypeId here is a placeholder as Gateway doesn't filter by it for reception.
    _communicator = new Comms(protocol, addr, ComponentType::GATEWAY, DataTypeId::UNKNOWN); 
    set_address(addr); // Ensure component base knows its actual listening address.
    
    db<GatewayComponent>(INF) << "[Gateway] " << getName() << " initialized on port " << PORT << "\n";
}

void GatewayComponent::run() {
    db<GatewayComponent>(INF) << "[Gateway] " << getName() << " starting main run loop.\n";
    
    // Create a properly initialized Message pointer that we'll use to receive messages
    Message* received_msg = nullptr;
    while (running()) { // Use Component's _running flag
        // Allocate a new Message for each receive call
        received_msg = new Message(Message::Type::UNKNOWN, Address(), DataTypeId::UNKNOWN);
        
        if (receive(received_msg) >= 0) { // Use Component's receive method and check for non-negative return value
            db<GatewayComponent>(INF) << "Gateway received msg on Port " << received_msg->origin().port()
                                      << ", type: " << static_cast<int>(received_msg->message_type()) 
                                      << ", unit_type: " << static_cast<int>(received_msg->unit_type())
                                      << " from " << received_msg->origin().to_string() << ". Relaying to Internal Broadcast Port "
                                      << Component::INTERNAL_BROADCAST_PORT << ".";
            
            // Construct the destination address for internal broadcast
            Address internal_broadcast_addr(_vehicle->address(), Component::INTERNAL_BROADCAST_PORT);
            
            db<GatewayComponent>(TRC) << "Gateway attempting to relay message via communicator to " << internal_broadcast_addr.to_string();

            // Use the communicator to send the original message to the internal broadcast address.
            // The Protocol layer is expected to handle this as a special case for relaying
            // by distributing the buffer of 'received_msg' to Port 1 observers.
            // The origin of the message will be preserved as it's part of received_msg.
            if (!_communicator->send(*received_msg, internal_broadcast_addr)) {
                db<GatewayComponent>(ERR) << "Gateway failed to relay message to " << internal_broadcast_addr.to_string()
                                          << " for message from " << received_msg->origin().to_string();
            }
            
            // Clean up the received message
            delete received_msg;
            received_msg = nullptr;
        } else {
            // Clean up if receive failed
            delete received_msg;
            received_msg = nullptr;
            
            // Optional: add a small sleep if receive is non-blocking and returns 0 frequently
            // to prevent busy-waiting if that's the case.
            // If receive() blocks, this is not strictly necessary.
            // usleep(1000); // e.g., 1ms sleep
        }
    }
    
    // Final cleanup just in case
    if (received_msg) {
        delete received_msg;
    }
    
    db<GatewayComponent>(INF) << "[Gateway] " << getName() << " exiting main run loop.\n";
}

// Removed component_dispatcher_routine, handle_interest, and handle_response

const unsigned int GatewayComponent::PORT = 0; // Gateway always uses Port 0

#endif // GATEWAY_COMPONENT_H