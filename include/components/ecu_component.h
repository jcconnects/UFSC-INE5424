#ifndef ECU_COMPONENT_H
#define ECU_COMPONENT_H

#include <chrono>
#include <string>
#include <mutex>
#include <vector>

#include "component.h"
#include "debug.h"
#include "teds.h"

class ECUComponent : public Component {
    public:
        // ECU receives a port for identification
        ECUComponent(Vehicle* vehicle, const unsigned int vehicle_id, const std::string& name, 
                    VehicleProt* protocol, Vehicle::Ports port = Vehicle::Ports::ECU1);

        ~ECUComponent() = default;

        // The main loop for the ECU component thread
        void run() override;

    private:
        // Interest period for requesting obstacle data (in microseconds)
        static const std::uint32_t OBSTACLE_DATA_PERIOD_US = 300000; // 300ms
        
        // Latest received obstacle data
        struct {
            float distance_meters = 0.0f;
            float angle_degrees = 0.0f;
            uint8_t confidence = 0;
            bool data_valid = false;
            std::chrono::time_point<std::chrono::high_resolution_clock> last_update;
            std::mutex mutex;
        } _latest_obstacle_data;
        
        // Handler for received obstacle data messages
        void handle_obstacle_data(const Message& message);
        
        // Serialization/deserialization helpers for obstacle data
        struct ObstacleData {
            float distance_meters;
            float angle_degrees;
            uint8_t confidence;
        };
        
        // Parse obstacle data from message
        bool parse_obstacle_data(const Message& message, ObstacleData& out_data);
        
        // Helper methods to process received messages 
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
        _log_file << "timestamp_us,received_distance_m,received_angle_deg,received_confidence,validity\n";
        _log_file.flush();
    }

    // Sets own address
    Address addr(vehicle->address(), static_cast<unsigned int>(port));

    // Sets own communicator
    _communicator = new Comms(protocol, addr);
    if (_communicator) {
        _communicator->set_owner_component(this);
    }
    
    db<ECUComponent>(INF) << "ECU Component initialized, ready to register interests\n";
}

void ECUComponent::run() {
    db<ECUComponent>(INF) << "ECU component " << getName() << " starting main run loop.\n";
    
    // Register interest in obstacle distance data
    register_interest_handler(
        DataTypeId::OBSTACLE_DISTANCE, 
        OBSTACLE_DATA_PERIOD_US,
        [this](const Message& msg) { 
            this->handle_obstacle_data(msg);
        }
    );
    
    db<ECUComponent>(INF) << "ECU registered interest in OBSTACLE_DISTANCE with period " 
                         << OBSTACLE_DATA_PERIOD_US << " microseconds\n";
    
    // Main loop - process and display received data periodically
    while (running()) {
        // Get latest obstacle data
        ObstacleData current_data;
        bool data_valid = false;
        std::chrono::microseconds data_age(0);
        
        {
            std::lock_guard<std::mutex> lock(_latest_obstacle_data.mutex);
            
            data_valid = _latest_obstacle_data.data_valid;
            if (data_valid) {
                current_data.distance_meters = _latest_obstacle_data.distance_meters;
                current_data.angle_degrees = _latest_obstacle_data.angle_degrees;
                current_data.confidence = _latest_obstacle_data.confidence;
                
                auto now = std::chrono::high_resolution_clock::now();
                data_age = std::chrono::duration_cast<std::chrono::microseconds>(
                    now - _latest_obstacle_data.last_update);
            }
        }
        
        // Log current data
        if (_log_file.is_open()) {
            auto now = std::chrono::high_resolution_clock::now();
            auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                now.time_since_epoch()).count();
                
            _log_file << now_us << ",";
            
            if (data_valid) {
                _log_file << current_data.distance_meters << ","
                         << current_data.angle_degrees << ","
                         << static_cast<int>(current_data.confidence) << ","
                         << "valid";
            } else {
                _log_file << "0,0,0,invalid";
            }
            
            _log_file << "\n";
            _log_file.flush();
        }
        
        // Display current status
        if (data_valid) {
            db<ECUComponent>(INF) << "ECU " << getName() << " processed obstacle data: dist=" 
                                 << current_data.distance_meters << "m, angle=" 
                                 << current_data.angle_degrees << "°, conf="
                                 << static_cast<int>(current_data.confidence) 
                                 << "%, age=" << data_age.count() << "us\n";
            
            // Here the ECU would perform vehicle control based on the obstacle data
            if (current_data.distance_meters < 10.0f && current_data.confidence > 80) {
                db<ECUComponent>(INF) << "ECU " << getName() << " ALERT: Obstacle within 10m - taking action!\n";
                // Simulate vehicle control action
            }
        } else {
            db<ECUComponent>(INF) << "ECU " << getName() << " waiting for obstacle data...\n";
        }
        
        // Sleep a bit
        usleep(500000); // 500ms
    }
    
    db<ECUComponent>(INF) << "ECU component " << getName() << " exiting main run loop.\n";
}

void ECUComponent::handle_obstacle_data(const Message& message) {
    // Parse the obstacle data from the message
    ObstacleData data;
    if (!parse_obstacle_data(message, data)) {
        db<ECUComponent>(WRN) << "ECU received invalid obstacle data message\n";
        return;
    }
    
    // Update the latest obstacle data
    {
        std::lock_guard<std::mutex> lock(_latest_obstacle_data.mutex);
        _latest_obstacle_data.distance_meters = data.distance_meters;
        _latest_obstacle_data.angle_degrees = data.angle_degrees;
        _latest_obstacle_data.confidence = data.confidence;
        _latest_obstacle_data.data_valid = true;
        _latest_obstacle_data.last_update = std::chrono::high_resolution_clock::now();
    }
    
    // Log the received message
    auto now = std::chrono::high_resolution_clock::now();
    auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
    
    std::string details = "distance=" + std::to_string(data.distance_meters) + 
                         ", angle=" + std::to_string(data.angle_degrees) + 
                         ", confidence=" + std::to_string(data.confidence);
    
    log_message(message, now_us, std::chrono::microseconds(message.timestamp()), details);
    
    db<ECUComponent>(TRC) << "ECU updated obstacle data from message\n";
}

bool ECUComponent::parse_obstacle_data(const Message& message, ObstacleData& out_data) {
    // Verify this is the right message type and has valid data
    if (message.type() != Message::Type::RESPONSE || 
        message.unit_type() != DataTypeId::OBSTACLE_DISTANCE) {
        return false;
    }
    
    // Get the message value data
    const void* value_data = message.value();
    std::size_t value_size = message.value_size();
    
    // Check size matches our struct
    if (!value_data || value_size < sizeof(ObstacleData)) {
        return false;
    }
    
    // Copy the data 
    std::memcpy(&out_data, value_data, sizeof(ObstacleData));
    return true;
}

void ECUComponent::log_message(const Message& message, const std::chrono::microseconds& recv_time,
                              const std::chrono::microseconds& timestamp, const std::string& message_details) {
    auto message_type = message.type();
    auto origin = message.origin();
    auto type_id = message.unit_type();
    auto latency_us = recv_time.count() - timestamp.count();
    
    // Log detailed debug info
    std::string message_type_str = (message_type == Message::Type::INTEREST) ? "INTEREST" : 
                                  (message_type == Message::Type::RESPONSE) ? "RESPONSE" : "OTHER";
    
    db<ECUComponent>(TRC) << "ECU " << getName() << " received " << message_type_str 
                         << " from " << origin.to_string()
                         << " for type " << static_cast<int>(type_id)
                         << " with latency " << latency_us << "us: " 
                         << message_details << "\n";
}
#endif // ECU_COMPONENT_H 