#ifndef GATEWAY_COMPONENT_H
#define GATEWAY_COMPONENT_H

#include <chrono>
#include <string>
#include <map>
#include <vector>

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
        // Override component_dispatcher_routine to directly handle Gateway-specific messages
        void component_dispatcher_routine();
        
    private:
        // Registry mapping DataTypeId to producer ports
        std::map<DataTypeId, std::vector<Port>> _producer_registry;
        
        // Helper method to process and log received messages
        void process_message(const Message& message, const std::chrono::microseconds& recv_time);
        
        // Helper methods to handle specific message types
        void handle_reg_producer(const Message& message);
        void handle_interest(const Message& message);
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
    
    db<GatewayComponent>(INF) << "Gateway component initialized on port " << PORT << "\n";
}

void GatewayComponent::run() {
    db<GatewayComponent>(INF) << "Gateway component " << getName() << " starting main run loop.\n";
    
    // The main Gateway logic is handled in component_dispatcher_routine override
    while (running()) {
        // This thread just stays alive and occasionally logs status
        usleep(1000000); // Sleep for 1 second
        
        // Log current registry status
        for (const auto& entry : _producer_registry) {
            db<GatewayComponent>(INF) << "Registry: Type " << static_cast<int>(entry.first) 
                                     << " has " << entry.second.size() << " producers\n";
        }
    }
    
    db<GatewayComponent>(INF) << "Gateway component " << getName() << " exiting main run loop.\n";
}

void GatewayComponent::component_dispatcher_routine() {
    db<GatewayComponent>(TRC) << "Gateway " << getName() << " dispatcher routine started.\n";
    
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
                db<GatewayComponent>(ERR) << "Gateway " << getName() << " dispatcher receive error: " << recv_size << "\n";
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
            if (message.message_type() == Message::Type::REG_PRODUCER) {
                handle_reg_producer(message);
            } 
            else if (message.message_type() == Message::Type::INTEREST) {
                handle_interest(message);
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
            db<GatewayComponent>(ERR) << "Gateway " << getName() << " dispatcher exception: " << e.what() << "\n";
        }
    }
    
    db<GatewayComponent>(TRC) << "Gateway " << getName() << " dispatcher routine exiting.\n";
}

void GatewayComponent::process_message(const Message& message, const std::chrono::microseconds& recv_time) {
    // Process message based on its type
    switch (message.message_type()) {
        case Message::Type::REG_PRODUCER:
            handle_reg_producer(message);
            break;
            
        case Message::Type::INTEREST:
            handle_interest(message);
            break;
            
        default:
            // Not a message type handled by Gateway
            db<GatewayComponent>(WRN) << "Gateway received unhandled message type: " 
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

void GatewayComponent::handle_reg_producer(const Message& message) {
    // Extract the DataTypeId being produced
    DataTypeId produced_type = message.unit_type();
    
    // Extract the producer's port from the message's origin
    Port producer_port = message.origin().port();
    
    // Add to registry (checking for duplicates)
    bool already_registered = false;
    auto& ports = _producer_registry[produced_type];
    
    for (const auto& port : ports) {
        if (port == producer_port) {
            already_registered = true;
            break;
        }
    }
    
    if (!already_registered) {
        ports.push_back(producer_port);
        db<GatewayComponent>(INF) << "Registered producer for type " << static_cast<int>(produced_type) 
                                 << " on port " << producer_port << " from " 
                                 << message.origin().to_string() << "\n";
    } else {
        db<GatewayComponent>(INF) << "Producer already registered for type " << static_cast<int>(produced_type) 
                                 << " on port " << producer_port << "\n";
    }
}

void GatewayComponent::handle_interest(const Message& message) {
    // Extract the DataTypeId being requested
    DataTypeId requested_type = message.unit_type();
    
    // Extract the requested period
    std::uint32_t period = message.period();
    
    db<GatewayComponent>(INF) << "Gateway received INTEREST for type " << static_cast<int>(requested_type) 
                             << " with period " << period << " from " 
                             << message.origin().to_string() << "\n";
    
    // Check if we have any registered producers for this type
    if (_producer_registry.count(requested_type) > 0) {
        const auto& producer_ports = _producer_registry[requested_type];
        
        if (producer_ports.empty()) {
            db<GatewayComponent>(WRN) << "No producers registered for type " 
                                     << static_cast<int>(requested_type) << "\n";
            return;
        }
        
        // Relay the interest to each registered producer
        for (const auto& target_producer_port : producer_ports) {
            // Create a new interest message
            Message internal_interest = _communicator->new_message(
                Message::Type::INTEREST,
                requested_type,
                period
            );
            
            // Send to the specific producer
            Address target(_vehicle->address(), target_producer_port);
            _communicator->send(internal_interest, target);
            
            db<GatewayComponent>(INF) << "Gateway relayed INTEREST for type " 
                                     << static_cast<int>(requested_type) 
                                     << " to internal port " << target_producer_port << "\n";
        }
    } else {
        db<GatewayComponent>(WRN) << "Gateway: No local producer registered for type " 
                                 << static_cast<int>(requested_type) << "\n";
    }
}

#endif // GATEWAY_COMPONENT_H