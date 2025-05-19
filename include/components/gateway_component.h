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
        // Define new standardized log header
        _log_file << "timestamp_us,event_category,event_type,message_id,message_type,data_type_id,origin_address,destination_address,period_us,value_size,notes\n";
        _log_file.flush();
    }

    db<GatewayComponent>(INF) << "[Gateway] Log created with new header.\n";

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
    
    Message received_msg(Message::Type::UNKNOWN, Address(), DataTypeId::UNKNOWN, 0); // Default constructor
    while (running()) {
        if (_communicator && _communicator->receive(&received_msg)) {
            auto now_reception_us = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()).count();

            // Log the received message by the Gateway
            if (_log_file.is_open()) {
                _log_file << now_reception_us << ","              // timestamp_us
                         << "GATEWAY" << ","                     // event_category
                         << "MSG_RECEIVED" << ","                // event_type
                         << received_msg.timestamp() << ","      // message_id (using original msg timestamp)
                         << static_cast<int>(received_msg.message_type()) << "," // message_type
                         << static_cast<int>(received_msg.unit_type()) << ","    // data_type_id
                         << received_msg.origin().to_string() << ","             // origin_address
                         << address().to_string() << ","                         // destination_address (Gateway's own)
                         << (received_msg.message_type() == Message::Type::INTEREST ? std::to_string(received_msg.period()) : "0") << "," // period_us
                         << (received_msg.message_type() == Message::Type::RESPONSE ? std::to_string(received_msg.value_size()) : "0") << "," // value_size
                         << "Received on port 0"                 // notes
                         << "\n";
                _log_file.flush();
            }
            
            db<GatewayComponent>(INF) << "[GatewayComponent] " << getName() << " received msg on Port 0 (" 
                                      << address().to_string() << "), "
                                      << "type: " << static_cast<int>(received_msg.message_type()) 
                                      << ", unit_type: " << static_cast<int>(received_msg.unit_type())
                                      << ", origin: " << received_msg.origin().to_string()
                                      << ". Processing for targeted relay.\n"; // Corrected log message

            // Targeted relay logic
            if (received_msg.message_type() == Message::Type::INTEREST) {
                db<GatewayComponent>(INF) << "[GatewayComponent] " << getName() << " received INTEREST for type " 
                                          << static_cast<int>(received_msg.unit_type()) 
                                          << ". Forwarding to relevant producers.\n";
                for (const auto& comp_info : _known_vehicle_components) {
                    if (comp_info.role == ComponentType::PRODUCER && comp_info.dataType == received_msg.unit_type()) {
                        db<GatewayComponent>(TRC) << "[GatewayComponent] Forwarding INTEREST to producer at " << comp_info.address.to_string() << "\n";
                        // Create a new message for sending using the Communicator's new_message method
                        Message interest_to_relay = _communicator->new_message(
                            Message::Type::INTEREST,
                            received_msg.unit_type(),
                            received_msg.period()
                        );

                        if (_communicator->send(interest_to_relay, comp_info.address)) {
                            db<GatewayComponent>(TRC) << "[GatewayComponent] Successfully relayed INTEREST to " << comp_info.address.to_string() << "\n";
                            // Log MSG_SENT for the relayed INTEREST message
                            if (_log_file.is_open()) {
                                auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                                    std::chrono::high_resolution_clock::now().time_since_epoch())
                                                    .count();
                                _log_file << now_us << ","
                                        << "GATEWAY" << ","
                                        << "MSG_SENT" << ","
                                        << interest_to_relay.timestamp() << "," // Use timestamp of the relayed message
                                        << static_cast<int>(interest_to_relay.message_type()) << "," 
                                        << static_cast<int>(interest_to_relay.unit_type()) << "," 
                                        << interest_to_relay.origin().to_string() << ","      // Gateway's address
                                        << comp_info.address.to_string() << ","  // Producer's address (the actual destination)
                                        << interest_to_relay.period() << ","              // Period
                                        << interest_to_relay.value_size() << ","        // Value size (0 for INTEREST)
                                        << "Relayed INTEREST (orig_msg_id: " << received_msg.timestamp() << ")" << "\n"; // Fixed the notes format
                                _log_file.flush();
                            }
                        } else {
                            db<GatewayComponent>(ERR) << "[GatewayComponent] Failed to forward INTEREST to producer at " << comp_info.address.to_string() << "\n";
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
                             // Log sent/relayed message
                            if (_log_file.is_open()) {
                                auto now_send_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                    std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                                _log_file << now_send_us << ","              // timestamp_us
                                         << "GATEWAY" << ","                 // event_category
                                         << "MSG_SENT" << ","                // event_type
                                         << received_msg.timestamp() << ","  // message_id (original)
                                         << static_cast<int>(received_msg.message_type()) << ","
                                         << static_cast<int>(received_msg.unit_type()) << ","
                                         << address().to_string() << ","     // origin_address (Gateway's own)
                                         << comp_info.address.to_string() << "," // destination_address
                                         << "0" << ","                       // period_us
                                         << received_msg.value_size() << "," // value_size
                                         << "Relayed RESPONSE"              // notes
                                         << "\n";
                                _log_file.flush();
                            }
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