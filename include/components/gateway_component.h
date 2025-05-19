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

    // Gateway listens on GATEWAY_PORT (0)
    Address addr(_vehicle->address(), GATEWAY_PORT); 

    db<GatewayComponent>(INF) << "[Gateway] Address set to " << addr.to_string() << "\n";

    // Initialize communicator, passing 'this', its type, and UNKNOWN as DataTypeId
    _communicator = new Comms(protocol, addr, ComponentType::GATEWAY, DataTypeId::UNKNOWN);
    set_address(addr); // Set the component's address
    
    db<GatewayComponent>(INF) << "[Gateway] " << getName() << " initialized on port " << PORT << "\n";
}

void GatewayComponent::run() {
    db<GatewayComponent>(INF) << "[GatewayComponent] " << getName() << " starting main run loop.\n";
    
    // Initialize with a valid public constructor to avoid using the private default constructor
    Message received_msg(Message::Type::UNKNOWN, Address(), DataTypeId::UNKNOWN, 0);
    while (running()) {
        // Blocking receive call - unblocks when a message is available via Communicator's update->Observer::update
        if (_communicator && _communicator->receive(&received_msg)) {
            db<GatewayComponent>(INF) << "[GatewayComponent] " << getName() << " received msg on Port 0 (" 
                                      << address().to_string() << "), "
                                      << "type: " << static_cast<int>(received_msg.message_type()) 
                                      << ", unit_type: " << static_cast<int>(received_msg.unit_type())
                                      << ", origin: " << received_msg.origin().to_string() // Origin from Communicator::receive
                                      << ". Relaying to internal broadcast (Port 1).\n";
            
            if (_log_file.is_open()) {
                auto now = std::chrono::high_resolution_clock::now();
                auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                    now.time_since_epoch()).count();
                _log_file << now_us << ","
                         << static_cast<int>(received_msg.message_type()) << ","
                         << static_cast<int>(received_msg.unit_type()) << ","
                         << received_msg.origin().to_string() << ","
                         << Component::INTERNAL_BROADCAST_PORT << "\n"; // Use Component:: for port const
                _log_file.flush();
            }

            // Relay the *original message data* to internal broadcast.
            // The Gateway's address is the 'from' for the protocol send.
            // The destination is its own MAC on INTERNAL_BROADCAST_PORT.
            // The payload is the raw data of the message just received.
            Address internal_broadcast_dest_addr(_vehicle->address(), Component::INTERNAL_BROADCAST_PORT);

            if (_vehicle && _vehicle->protocol()) {
                // Send the raw serialized data of the original message`
                _communicator->send(
                    received_msg,  // The original message
                    internal_broadcast_dest_addr  // To: MAC_VEHICLE:Port_1
                );
                db<GatewayComponent>(TRC) << "[GatewayComponent] " << getName() << " relayed message of size " 
                                          << received_msg.size() << " to " << internal_broadcast_dest_addr.to_string();
            } else db<GatewayComponent>(ERR) << "[GatewayComponent] " << getName() << " cannot relay: vehicle or protocol is null.\n";

        } else {
            if (!running()) {
                db<GatewayComponent>(INF) << "[GatewayComponent] " << getName() << " no longer running, exiting receive loop.\n";
                break; 
            }
            // Communicator::receive blocks, so this branch is primarily hit when communicator is closed.
        }
    }
    db<GatewayComponent>(INF) << "[GatewayComponent] " << getName() << " exiting main run loop.\n";
}

// Removed component_dispatcher_routine, handle_interest, and handle_response

const unsigned int GatewayComponent::PORT = 0; // Gateway always uses Port 0

#endif // GATEWAY_COMPONENT_H