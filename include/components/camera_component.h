#ifndef CAMERA_COMPONENT_H
#define CAMERA_COMPONENT_H

#include <chrono>
#include <string>
#include <vector>
#include <random>

#include "component.h"
#include "debug.h"
#include "teds.h"

class CameraComponent : public Component {
    public:
        // Define the port
        static const unsigned int PORT;
        
        // Camera temperature data structure
        struct TemperatureData {
            float temperature_celsius;
            float humidity_percent;
            uint8_t status; // 0-255 (0=error, 1=normal, 2=warning, 3=critical)
            
            // Serialization helper
            static std::vector<std::uint8_t> serialize(const TemperatureData& data) {
                std::vector<std::uint8_t> result(sizeof(TemperatureData));
                std::memcpy(result.data(), &data, sizeof(TemperatureData));
                return result;
            }
            
            // Deserialization helper
            static TemperatureData deserialize(const std::vector<std::uint8_t>& bytes) {
                TemperatureData data;
                if (bytes.size() >= sizeof(TemperatureData)) {
                    std::memcpy(&data, bytes.data(), sizeof(TemperatureData));
                }
                return data;
            }
        };

        CameraComponent(Vehicle* vehicle, const unsigned int vehicle_id, 
                       const std::string& name, VehicleProt* protocol);

        ~CameraComponent() = default;

        void run() override;
        
    protected:
        // Override produce_data_for_response to generate temperature data
        bool produce_data_for_response(DataTypeId type, std::vector<std::uint8_t>& out_value) override;
        
    private:
        // Random number generator for simulated data
        std::mt19937 _rng;
        std::uniform_real_distribution<float> _temp_dist;  // Temperature distribution
        std::uniform_real_distribution<float> _humidity_dist;  // Humidity distribution
        std::uniform_int_distribution<uint8_t> _status_dist;  // Status distribution
        
        // Current temperature data
        TemperatureData _current_data;
        
        // Update the simulated temperature data
        void update_temperature_data();
};

/******** Camera Component Implementation *******/
const unsigned int CameraComponent::PORT = 102; // Example port for Camera

CameraComponent::CameraComponent(Vehicle* vehicle, const unsigned int vehicle_id, 
                                const std::string& name, VehicleProt* protocol) 
    : Component(vehicle, vehicle_id, name),
      _rng(std::random_device{}()),
      _temp_dist(35.0f, 80.0f),      // 35-80°C (cameras can get hot)
      _humidity_dist(30.0f, 70.0f),  // 30-70% humidity
      _status_dist(1, 3)             // Status: 1=normal, 2=warning, 3=critical
{
    // Initialize as a producer of TEMPERATURE_SENSOR data
    _produced_data_type = DataTypeId::TEMPERATURE_SENSOR;
    
    // Set up logging
    open_log_file();
    if (_log_file.is_open()) {
        _log_file.seekp(0);
        _log_file << "timestamp_us,temperature_celsius,humidity_percent,status\n";
        _log_file.flush();
    }

    // Initialize with a random temperature data
    update_temperature_data();
    
    // Set up communicator with camera port
    Address addr(_vehicle->address().paddr(), PORT);
    _communicator = new Comms(protocol, addr);
    if (_communicator) {
        _communicator->set_owner_component(this);
    }
    
    db<CameraComponent>(INF) << "Camera Component initialized as producer of type " 
                           << static_cast<int>(_produced_data_type) << "\n";
}

void CameraComponent::run() {
    db<CameraComponent>(INF) << "Camera component " << getName() << " starting main run loop.\n";
    
    // Send REG_PRODUCER message to Gateway
    Message reg_msg = _communicator->new_message(
        Message::Type::REG_PRODUCER,
        _produced_data_type
    );
    
    // Send to Gateway (port 0)
    Address gateway_addr(_vehicle->address().paddr(), 0);
    _communicator->send(reg_msg, gateway_addr);
    
    db<CameraComponent>(INF) << "Camera sent REG_PRODUCER for type " 
                           << static_cast<int>(_produced_data_type) 
                           << " to Gateway.\n";
    
    // Main loop - update sensor data periodically
    // Note: The actual periodic response sending is handled by producer_response_thread
    while (running()) {
        // Update simulated temperature data
        update_temperature_data();
        
        // Log current data
        if (_log_file.is_open()) {
            auto now = std::chrono::high_resolution_clock::now();
            auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                now.time_since_epoch()).count();
                
            _log_file << now_us << ","
                     << _current_data.temperature_celsius << ","
                     << _current_data.humidity_percent << ","
                     << static_cast<int>(_current_data.status) << "\n";
            _log_file.flush();
        }
        
        // Sleep for a bit - actual response rate is determined by consumer periods
        usleep(150000); // 150ms
    }
    
    db<CameraComponent>(INF) << "Camera component " << getName() << " exiting main run loop.\n";
}

void CameraComponent::update_temperature_data() {
    // Generate new simulated data
    _current_data.temperature_celsius = _temp_dist(_rng);
    _current_data.humidity_percent = _humidity_dist(_rng);
    _current_data.status = _status_dist(_rng);
    
    // Adjust status based on temperature (higher temp = higher chance of warning/critical)
    if (_current_data.temperature_celsius > 70.0f) {
        _current_data.status = 3; // Critical
    } else if (_current_data.temperature_celsius > 60.0f) {
        _current_data.status = 2; // Warning
    }
    
    db<CameraComponent>(TRC) << "Camera updated temperature data: temp=" 
                           << _current_data.temperature_celsius << "°C, humidity="
                           << _current_data.humidity_percent << "%, status="
                           << static_cast<int>(_current_data.status) << "\n";
}

bool CameraComponent::produce_data_for_response(DataTypeId type, std::vector<std::uint8_t>& out_value) {
    // Only respond to requests for our produced data type
    if (type != DataTypeId::TEMPERATURE_SENSOR) {
        return false;
    }
    
    // Serialize the current temperature data
    out_value = TemperatureData::serialize(_current_data);
    
    db<CameraComponent>(INF) << "Camera produced temperature data with size " << out_value.size() << " bytes\n";
    return true;
}

#endif // CAMERA_COMPONENT_H 