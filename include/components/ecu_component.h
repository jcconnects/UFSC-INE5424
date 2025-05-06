#ifndef ECU_COMPONENT_H
#define ECU_COMPONENT_H

#include <chrono>
#include <string>

#include "component.h"
#include "debug.h"

class ECUComponent : public Component {
    public:
        // ECU receives a port for identification
        ECUComponent(Vehicle* vehicle, const unsigned int vehicle_id, const std::string& name, 
                    VehicleProt* protocol, Vehicle::Ports port = Vehicle::Ports::ECU1);

        ~ECUComponent() = default;

        // The main loop for the ECU component thread
        void run() override;

    private:
        // Helper methods to process received messages
        void process_message(const Message& message, const std::chrono::microseconds& recv_time);
        void process_interest_message(const Message& message, std::string& message_details);
        void process_response_message(const Message& message, std::string& message_details);
        void log_message(const Message& message, const std::chrono::microseconds& recv_time, 
                        const std::chrono::microseconds& timestamp, const std::string& message_details);
};

/*********** ECU Component Implementation **********/
ECUComponent::ECUComponent(Vehicle* vehicle, const unsigned int vehicle_id, const std::string& name, 
                           VehicleProt* protocol, Vehicle::Ports port) 
    : Component(vehicle, vehicle_id, name) 
{
    // Sets CSV result Header
    open_log_file();
    if (_log_file.is_open()) {
        _log_file.seekp(0); // Go to beginning to overwrite if file exists
        // Define log header
        _log_file << "receive_timestamp_us,source_address,message_type,type_id,event_type,"
                  << "send_timestamp_us,latency_us,message_details\n";
        _log_file.flush();
    }

    // Sets own address
    Address addr(vehicle->address(), static_cast<unsigned int>(port));

    // Sets own communicator
    _communicator = new Comms(protocol, addr);
}

void ECUComponent::run() {
    db<ECUComponent>(INF) << "[ECUComponent] " << Component::getName() << " thread running.\n";

    while (running()) {
        // Create a message object to receive into (needs to be created with new_message)
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

    db<ECUComponent>(INF) << "[ECUComponent] " << Component::getName() << " thread terminated.\n";
}

void ECUComponent::process_message(const Message& message, const std::chrono::microseconds& recv_time) {
    // Extract message details
    auto message_type = message.message_type();
    auto origin = message.origin();
    auto timestamp = message.timestamp();
    auto type_id = message.unit_type();
    
    // Calculate latency
    auto latency_us = recv_time.count() - timestamp;
    
    // Log basic message information
    db<ECUComponent>(TRC) << "[ECUComponent] " << Component::getName() 
                         << " received message: type=" << (message_type == Message::Type::INTEREST ? "INTEREST" : "RESPONSE")
                         << ", from=" << origin.to_string()
                         << ", type_id=" << type_id
                         << ", latency=" << latency_us << "us\n";
    
    // Process based on message type
    std::string message_details;
    if (message_type == Message::Type::INTEREST) {
        process_interest_message(message, message_details);
    } else if (message_type == Message::Type::RESPONSE) {
        process_response_message(message, message_details);
    }
    
    // Log the message to CSV
    log_message(message, recv_time, std::chrono::microseconds(timestamp), message_details);
}

void ECUComponent::process_interest_message(const Message& message, std::string& message_details) {
    auto period = message.period();
    message_details = "period=" + std::to_string(period) + "ms";
    
    db<ECUComponent>(INF) << "[ECUComponent] " << Component::getName()
                         << " Received INTEREST message with period " << period << "ms\n";
    
    // Handle interest message - in a real implementation, we might set up a periodic response
}

void ECUComponent::process_response_message(const Message& message, std::string& message_details) {
    // Get the value data
    const std::uint8_t* value_data = message.value();
    
    // Check if we have value data
    if (value_data != nullptr) {
        // Estimate the value size - in a real implementation, you would know this from the Message class
        // For now, we'll use a safe approach for logging
        const size_t max_log_len = 16; // Maximum bytes to log
        
        // Create a safe string from the value data (limiting length for logging)
        std::string value_str;
        size_t i = 0;
        while (value_data[i] != 0 && i < max_log_len) {
            // Only include printable characters in the log
            if (value_data[i] >= 32 && value_data[i] <= 126) {
                value_str += static_cast<char>(value_data[i]);
            } else {
                value_str += '?'; // Replace non-printable with ?
            }
            i++;
        }
        
        message_details = "value=\"" + value_str + (i >= max_log_len ? "..." : "") + "\"";
        
        db<ECUComponent>(INF) << "[ECUComponent] " << Component::getName()
                             << " Received RESPONSE message with value data\n";
    }
    else {
        message_details = "value=null";
        db<ECUComponent>(INF) << "[ECUComponent] " << Component::getName()
                             << " Received RESPONSE message with no value data\n";
    }
}

void ECUComponent::log_message(const Message& message, const std::chrono::microseconds& recv_time,
                              const std::chrono::microseconds& timestamp, const std::string& message_details) {
    auto message_type = message.message_type();
    auto origin = message.origin();
    auto type_id = message.unit_type();
    auto latency_us = recv_time.count() - timestamp.count();
    
    // Log to CSV if file is open
    std::string message_type_str = (message_type == Message::Type::INTEREST) ? "INTEREST" : "RESPONSE";
    
    if (_log_file.is_open()) {
        try {
            _log_file << recv_time.count() << ","
                     << origin.to_string() << ","
                     << message_type_str << ","
                     << type_id << ",receive,"
                     << timestamp.count() << ","
                     << latency_us << ","
                     << "\"" << message_details << "\"\n";
            _log_file.flush();
        } catch (const std::exception& e) {
            db<ECUComponent>(ERR) << "[ECUComponent] " << Component::getName() 
                                 << " error writing to log file: " << e.what() << "\n";
        }
    } else {
        // Log to console as a fallback
        db<ECUComponent>(TRC) << "[ECUComponent] " << Component::getName() 
                             << " CSV log data: " << recv_time.count() << ","
                             << origin.to_string() << ","
                             << message_type_str << ","
                             << type_id << ",receive,"
                             << timestamp.count() << ","
                             << latency_us << ","
                             << message_details << "\n";
    }
}
#endif // ECU_COMPONENT_H 