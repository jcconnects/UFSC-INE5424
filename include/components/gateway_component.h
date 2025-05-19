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
        // Struct for hardcoded component information
        struct KnownComponent {
            Address address; // This is Component::Address, which is Protocol::Address
            ComponentType role; // PRODUCER or CONSUMER
            DataTypeId dataType; // The type it produces or is interested in
        };
        std::vector<KnownComponent> _known_vehicle_components;

        // Removed: void handle_interest(const Message& message);
        // Removed: void handle_response(const Message& message);
};

// Move implementation to a separate .cpp file
#include "component.h"  // Now can safely include Component
#include "vehicle.h"    // For _vehicle->protocol()
// Required for BasicProducer::PORT and BasicConsumer::PORT
#include "components/basic_producer.h"
#include "components/basic_consumer.h"

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

    // Populate _known_vehicle_components (hardcoded)
    // BasicProducer
    _known_vehicle_components.push_back({
        Address(_vehicle->address(), BasicProducer::PORT),
        ComponentType::PRODUCER,
        DataTypeId::CUSTOM_SENSOR_DATA_A
    });
    // BasicConsumer
    _known_vehicle_components.push_back({
        Address(_vehicle->address(), BasicConsumer::PORT),
        ComponentType::CONSUMER,
        DataTypeId::CUSTOM_SENSOR_DATA_A
    });
    // Add other known components here if necessary

    db<GatewayComponent>(INF) << "[Gateway] Initialized with " << _known_vehicle_components.size() 
                             << " known components for targeted relay.\n";
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

            // Targeted relay logic instead of internal broadcast
            if (received_msg.message_type() == Message::Type::INTEREST) {
                db<GatewayComponent>(INF) << "[GatewayComponent] " << getName() << " received INTEREST for type " 
                                          << static_cast<int>(received_msg.unit_type()) 
                                          << ". Forwarding to relevant producers.\n";
                for (const auto& comp_info : _known_vehicle_components) {
                    if (comp_info.role == ComponentType::PRODUCER && comp_info.dataType == received_msg.unit_type()) {
                        if (_communicator->send(received_msg, comp_info.address)) {
                            db<GatewayComponent>(TRC) << "[GatewayComponent] " << getName() << " Forwarded INTEREST to producer at " 
                                                      << comp_info.address.to_string() << "\n";
                        } else {
                            db<GatewayComponent>(ERR) << "[GatewayComponent] " << getName() << " Failed to forward INTEREST to producer at " 
                                                      << comp_info.address.to_string() << "\n";
                        }
                    }
                }
            } else if (received_msg.message_type() == Message::Type::RESPONSE) {
                db<GatewayComponent>(INF) << "[GatewayComponent] " << getName() << " received RESPONSE for type " 
                                          << static_cast<int>(received_msg.unit_type()) 
                                          << ". Forwarding to relevant consumers.\n";
                for (const auto& comp_info : _known_vehicle_components) {
                    if (comp_info.role == ComponentType::CONSUMER && comp_info.dataType == received_msg.unit_type()) {
                        if (_communicator->send(received_msg, comp_info.address)) {
                            db<GatewayComponent>(TRC) << "[GatewayComponent] " << getName() << " Forwarded RESPONSE to consumer at " 
                                                      << comp_info.address.to_string() << "\n";
                        } else {
                            db<GatewayComponent>(ERR) << "[GatewayComponent] " << getName() << " Failed to forward RESPONSE to consumer at " 
                                                      << comp_info.address.to_string() << "\n";
                        }
                    }
                }
            } else {
                db<GatewayComponent>(WRN) << "[GatewayComponent] " << getName() << " received unhandled message type: " 
                                          << static_cast<int>(received_msg.message_type()) << "\n";
            }

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