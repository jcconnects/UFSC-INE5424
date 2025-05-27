#ifndef ECU_COMPONENT_H
#define ECU_COMPONENT_H

#include <chrono>
#include <string>

#include "../api/util/debug.h"
#include "api/framework/agent.h"
#include "api/network/bus.h"
#include "app/vehicle.h"
#include "app/datatypes.h"

class ECUComponent : public Agent {
    public:
        // ECU receives a port for identification
        ECUComponent(CAN* can, const std::string& name = "ECUComponent");
        ~ECUComponent() = default;

        void handle_response(Message* msg) override;
        void process_message(const Message& message, const std::chrono::microseconds& recv_time);

        Value get(Unit type) override { return Value(); } 

    private:
        // Helper methods to process received messages
        void process_interest_message(const Message& message, std::string& message_details);
        void process_response_message(const Message& message, std::string& message_details);
        void log_message(const Message& message, const std::chrono::microseconds& recv_time, 
                        const std::chrono::microseconds& timestamp, const std::string& message_details);
};

/*********** ECU Component Implementation **********/
ECUComponent::ECUComponent(CAN* can, const std::string& name) : Agent(can, name) {
    // TODO - review this so that ECU can receive other message types
    add_observed_type(static_cast<std::uint32_t>(DataTypes::EXTERNAL_POINT_CLOUD_XYZ), CAN::Message::Type::UNKNOWN);
}

void ECUComponent::handle_response(Message* msg) {
    auto recv_time = std::chrono::system_clock::now();
    auto recv_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
        recv_time.time_since_epoch());
    
    process_message((*msg), recv_time_us);
}

void ECUComponent::process_message(const Message& message, const std::chrono::microseconds& recv_time) {
    // Extract message details
    auto message_type = message.message_type();
    auto origin = message.origin();
    auto timestamp = message.timestamp();
    auto type_id = message.unit();
    
    // Calculate latency
    auto latency_us = recv_time.count() - timestamp.count();
    
    // Log basic message information
    db<ECUComponent>(TRC) << "[ECUComponent] " << name() 
                         << " received message: type=" << (message_type == Message::Type::INTEREST ? "INTEREST" : "RESPONSE")
                         << ", from=" << origin.to_string()
                         << ", type_id=" << type_id
                         << ", latency=" << latency_us << "us\n";
    
    // Process based on message type
    std::string message_details;
    if (message_type == Message::Type::RESPONSE) {
        process_response_message(message, message_details);
    }
    
    // Log the message to CSV
    log_message(message, recv_time, std::chrono::microseconds(timestamp), message_details);
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
        
        db<ECUComponent>(INF) << "[ECUComponent] " << name() 
                             << " Received RESPONSE message with value data\n";
    }
    else {
        message_details = "value=null";
        db<ECUComponent>(INF) << "[ECUComponent] " << name() 
                             << " Received RESPONSE message with no value data\n";
    }
}

void ECUComponent::log_message(const Message& message, const std::chrono::microseconds& recv_time,
                              const std::chrono::microseconds& timestamp, const std::string& message_details) {
    auto message_type = message.message_type();
    auto origin = message.origin();
    auto type_id = message.unit();
    auto latency_us = recv_time.count() - timestamp.count();
    
    // Log to CSV if file is open
    std::string message_type_str = (message_type == Message::Type::INTEREST) ? "INTEREST" : "RESPONSE";
    // Log to console
    db<ECUComponent>(TRC) << "[ECUComponent] " << name()  
                            << " CSV log data: " << recv_time.count() << ","
                            << origin.to_string() << ","
                            << message_type_str << ","
                            << type_id << ",receive,"
                            << timestamp.count() << ","
                            << latency_us << ","
                            << message_details << "\n";
}
#endif // ECU_COMPONENT_H 