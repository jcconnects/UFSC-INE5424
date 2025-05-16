#ifndef LIDAR_COMPONENT_H
#define LIDAR_COMPONENT_H

#include <chrono>
#include <string>
#include <array>
#include <cmath>
#include <random>
#include <vector>

#include "component.h"
#include "debug.h"
#include "teds.h"

class LidarComponent : public Component {
    public:
        // Define the port
        static const unsigned int PORT;
        
        // Lidar data structure
        struct ObstacleDistanceData {
            float distance_meters;
            float angle_degrees;
            uint8_t confidence; // 0-100
            
            // Serialization helper
            static std::vector<std::uint8_t> serialize(const ObstacleDistanceData& data) {
                std::vector<std::uint8_t> result(sizeof(ObstacleDistanceData));
                std::memcpy(result.data(), &data, sizeof(ObstacleDistanceData));
                return result;
            }
            
            // Deserialization helper
            static ObstacleDistanceData deserialize(const std::vector<std::uint8_t>& bytes) {
                ObstacleDistanceData data;
                if (bytes.size() >= sizeof(ObstacleDistanceData)) {
                    std::memcpy(&data, bytes.data(), sizeof(ObstacleDistanceData));
                }
                return data;
            }
        };

        LidarComponent(Vehicle* vehicle, const unsigned int vehicle_id, 
                       const std::string& name, VehicleProt* protocol);

        ~LidarComponent() = default;

        void run() override;
        
    protected:
        // Override produce_data_for_response to generate lidar data
        bool produce_data_for_response(DataTypeId type, std::vector<std::uint8_t>& out_value) override;
        
    private:
        // Random number generator for simulated data
        std::mt19937 _rng;
        std::uniform_real_distribution<float> _dist_dist; // Distance distribution
        std::uniform_real_distribution<float> _angle_dist; // Angle distribution 
        std::uniform_int_distribution<uint8_t> _confidence_dist; // Confidence distribution
        
        // Current obstacle data
        ObstacleDistanceData _current_data;
        
        // Update the simulated obstacle data
        void update_obstacle_data();
};

/******** Lidar Component Implementation *******/
const unsigned int LidarComponent::PORT = 101; // Example port for Lidar

LidarComponent::LidarComponent(Vehicle* vehicle, const unsigned int vehicle_id, 
                              const std::string& name, VehicleProt* protocol) 
    : Component(vehicle, vehicle_id, name, ComponentType::PRODUCER), 
      _rng(std::random_device{}()),
      _dist_dist(0.5f, 50.0f),       // 0.5 to 50 meters
      _angle_dist(-180.0f, 180.0f),  // -180 to 180 degrees
      _confidence_dist(60, 100)      // 60% to 100% confidence
{
    // Initialize as a producer of OBSTACLE_DISTANCE data
    _produced_data_type = DataTypeId::OBSTACLE_DISTANCE;
    
    // Set up logging
    open_log_file();
    if (_log_file.is_open()) {
        _log_file.seekp(0);
        _log_file << "timestamp_us,distance_m,angle_deg,confidence\n";
        _log_file.flush();
    }

    // Initialize with a random obstacle data
    update_obstacle_data();
    
    // Set up communicator with lidar port, passing 'this' to the constructor
    Address addr(_vehicle->address(), PORT);
    _communicator = new Comms(protocol, addr, this, ComponentType::PRODUCER, DataTypeId::OBSTACLE_DISTANCE);
    
    // IMPORTANT: Set up the interest period callback
    _communicator->set_interest_period_callback([this](std::uint32_t period) {
        this->handle_interest_period(period);
    });
    
    db<LidarComponent>(INF) << "Lidar Component initialized as producer of type " 
                           << static_cast<int>(_produced_data_type) << "\n";
}

void LidarComponent::run() {
    db<LidarComponent>(INF) << "Lidar component " << getName() << " starting main run loop.\n";
    
    // Main loop - update sensor data periodically
    // Note: The actual periodic response sending is handled by producer_response_thread
    while (running()) {
        // Update simulated obstacle data
        update_obstacle_data();
        
        // Log current data
        if (_log_file.is_open()) {
            auto now = std::chrono::high_resolution_clock::now();
            auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                now.time_since_epoch()).count();
                
            _log_file << now_us << ","
                     << _current_data.distance_meters << ","
                     << _current_data.angle_degrees << ","
                     << static_cast<int>(_current_data.confidence) << "\n";
            _log_file.flush();
        }
        
        // Sleep for a bit - actual response rate is determined by consumer periods
        usleep(100000); // 100ms
    }
    
    db<LidarComponent>(INF) << "Lidar component " << getName() << " exiting main run loop.\n";
}

void LidarComponent::update_obstacle_data() {
    // Generate new simulated data
    _current_data.distance_meters = _dist_dist(_rng);
    _current_data.angle_degrees = _angle_dist(_rng);
    _current_data.confidence = _confidence_dist(_rng);
    
    db<LidarComponent>(TRC) << "Lidar updated obstacle data: dist=" 
                           << _current_data.distance_meters << "m, angle="
                           << _current_data.angle_degrees << "°, conf="
                           << static_cast<int>(_current_data.confidence) << "%\n";
}

bool LidarComponent::produce_data_for_response(DataTypeId type, std::vector<std::uint8_t>& out_value) {
    // Only respond to requests for our produced data type
    if (type != DataTypeId::OBSTACLE_DISTANCE) {
        return false;
    }
    
    // Serialize the current obstacle data
    out_value = ObstacleDistanceData::serialize(_current_data);
    
    db<LidarComponent>(INF) << "Lidar produced data with size " << out_value.size() << " bytes\n";
    return true;
}

#endif // LIDAR_COMPONENT_H 