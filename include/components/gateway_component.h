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
        // Helper method to process and log received messages
        void process_message(const Message& message, const std::chrono::microseconds& recv_time);
        
        // Helper methods to handle specific message types
        void handle_interest(const Message& message);
        void handle_response(const Message& message);
        
        // Get producer port for a given data type using vehicle's hardcoded mappings
        std::uint16_t get_producer_port(DataTypeId type) const;
        
        // Get consumer ports interested in a given data type
        std::vector<std::uint16_t> get_consumer_ports(DataTypeId type) const;
        
        // Map to track which data types are requested by which consumer ports
        std::map<DataTypeId, std::vector<std::uint16_t>> _consumer_interests;
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
    std::uint8_t raw_buffer[1024]; // Adjust size as needed based on your MTU
    
    while (_dispatcher_running.load()) {
        // Receive raw message
        Address source;
        int recv_size = receive(raw_buffer, sizeof(raw_buffer), &source);
        
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
            
            // Deserialize raw message
            Message message = Message::deserialize(raw_buffer, recv_size);
            
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

void GatewayComponent::process_message(const Message& message, const std::chrono::microseconds& recv_time) {
    // Process message based on its type
    switch (message.message_type()) {
        case Message::Type::INTEREST:
            handle_interest(message);
            break;
            
        case Message::Type::RESPONSE:
            handle_response(message);
            break;
            
        default:
            // Not a message type handled by Gateway
            db<GatewayComponent>(WRN) << "[Gateway] received unhandled message type: " 
                                     << static_cast<int>(message.message_type()) << "\n";
            break;
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
}

void GatewayComponent::handle_interest(const Message& message) {
    DataTypeId requested_type = message.unit_type();
    std::uint32_t period = message.period();
    Address original_requester = message.origin(); // Capture original requester

    db<GatewayComponent>(INF) << "[Gateway] handle_interest: Received INTEREST for type " 
                             << static_cast<int>(requested_type) << " with period " << period 
                             << " from original requester: " << original_requester.to_string() << "\n";

    // Track consumer interest if the source port is not 0 (another Gateway)
    if (original_requester.port() != 0) {
        // Add to _consumer_interests if not already tracked
        auto& consumers = _consumer_interests[requested_type];
        if (std::find(consumers.begin(), consumers.end(), original_requester.port()) == consumers.end()) {
            consumers.push_back(original_requester.port());
            db<GatewayComponent>(INF) << "[Gateway] Tracking new consumer interest from port " 
                                     << original_requester.port() << " for type " 
                                     << static_cast<int>(requested_type) << "\n";
        }
    }
    
    // Get producer port for the requested data type using hardcoded mapping
    std::uint16_t producer_port = get_producer_port(requested_type);
    
    if (producer_port > 0) {  
        db<GatewayComponent>(INF) << "[Gateway] Found hardcoded producer for type " 
                               << static_cast<int>(requested_type) 
                               << " on port " << producer_port << ". Relaying interest.\n";
        
        Message internal_interest = _communicator->new_message(
            Message::Type::INTEREST,
            requested_type,
            period
        );
        // The origin of this internal_interest will be the Gateway's communicator address
        
        Address local_producer_addr(_vehicle->address(), producer_port);

        db<GatewayComponent>(INF) << "[Gateway] Relaying INTEREST for type " << static_cast<int>(requested_type)
                                 << " (original period: " << period << "us) to local producer on port " 
                                 << producer_port << " (address: " << local_producer_addr.to_string() << ")\n";
        
        bool send_success = _communicator->send(internal_interest, local_producer_addr);
        if (send_success) {
            db<GatewayComponent>(INF) << "[Gateway] Successfully sent relayed INTEREST to port " << producer_port << "\n";
        } else {
            db<GatewayComponent>(ERR) << "[Gateway] Failed to send relayed INTEREST to port " << producer_port << "\n";
        }
    } else {
        db<GatewayComponent>(INF) << "[Gateway] No hardcoded producer registered for type " << static_cast<int>(requested_type) 
                                 << ". Interest from " << original_requester.to_string() << " will not be relayed locally.\n";
    }
}

void GatewayComponent::handle_response(const Message& message) {
    DataTypeId response_type = message.unit_type();
    
    db<GatewayComponent>(INF) << "[Gateway] Received RESPONSE for data type " 
                             << static_cast<int>(response_type) 
                             << " from " << message.origin().to_string() << "\n";
    
    // Find consumers interested in this data type
    auto it = _consumer_interests.find(response_type);
    if (it != _consumer_interests.end() && !it->second.empty()) {
        // Get the value data from the original message
        const std::uint8_t* value_data = message.value();
        std::uint32_t value_size = message.value_size();
        
        db<GatewayComponent>(INF) << "[Gateway] Found " << it->second.size() 
                                 << " interested consumers for type " 
                                 << static_cast<int>(response_type)
                                 << " with data size " << value_size << " bytes\n";
        
        if (!value_data || value_size == 0) {
            db<GatewayComponent>(ERR) << "[Gateway] RESPONSE message has no value data. Cannot relay.\n";
            return;
        }
        
        // Create a new RESPONSE message to relay (with Gateway as origin)
        Message relayed_response = _communicator->new_message(
            Message::Type::RESPONSE,
            response_type,
            0, // No period for responses
            value_data,
            value_size
        );
        
        db<GatewayComponent>(INF) << "[Gateway] Relaying RESPONSE for type " 
                                 << static_cast<int>(response_type)
                                 << " to " << it->second.size() << " interested consumers\n";
        
        // Forward to each interested consumer
        for (const auto& consumer_port : it->second) {
            Address consumer_addr(_vehicle->address(), consumer_port);
            
            db<GatewayComponent>(INF) << "[Gateway] Sending relayed RESPONSE to consumer on port " 
                                     << consumer_port << "\n";
            
            bool send_success = _communicator->send(relayed_response, consumer_addr);
            if (send_success) {
                db<GatewayComponent>(INF) << "[Gateway] Successfully relayed RESPONSE to consumer on port " 
                                         << consumer_port << "\n";
            } else {
                db<GatewayComponent>(ERR) << "[Gateway] Failed to relay RESPONSE to consumer on port " 
                                         << consumer_port << "\n";
            }
        }
    } else {
        db<GatewayComponent>(INF) << "[Gateway] No local consumers interested in data type " 
                                 << static_cast<int>(response_type) << "\n";
    }
}

std::uint16_t GatewayComponent::get_producer_port(DataTypeId type) const {
    // Use Vehicle's hardcoded mapping to find the producer port for this data type
    auto producer_map = _vehicle->get_producer_ports();
    auto it = producer_map.find(type);
    if (it != producer_map.end()) {
        return static_cast<std::uint16_t>(it->second); // Convert enum to uint16_t
    }
    return 0; // Return 0 if no producer found (invalid port)
}

std::vector<std::uint16_t> GatewayComponent::get_consumer_ports(DataTypeId type) const {
    // Check if we have any consumers interested in this data type
    auto it = _consumer_interests.find(type);
    if (it != _consumer_interests.end()) {
        return it->second;
    }
    return {}; // Return empty vector if no consumers
}

#endif // GATEWAY_COMPONENT_H