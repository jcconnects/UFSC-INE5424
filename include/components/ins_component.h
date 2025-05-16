#ifndef INS_COMPONENT_H
#define INS_COMPONENT_H

#include <chrono>
#include <random>
#include <string>
#include <vector>
#include <iomanip> // For std::fixed, std::setprecision
#include <cmath>   // For math operations

#include "component.h"
#include "debug.h"
#include "teds.h"

// Constants
constexpr double PI_INS = 3.14159265358979323846;
constexpr double G_TO_MS2_INS = 9.80665;
constexpr double DEG_TO_RAD_INS = PI_INS / 180.0;


class INSComponent : public Component {
    public:
        static const unsigned int PORT;
        
        // INS position data structure
        struct GPSPositionData {
            double latitude_rad;   // Latitude in radians (-PI/2 to PI/2)
            double longitude_rad;  // Longitude in radians (-PI to PI)
            double altitude_m;     // Altitude in meters
            double velocity_mps;   // Velocity in meters per second
            double heading_rad;    // Heading in radians (0 to 2*PI)
            
            // Serialization helper
            static std::vector<std::uint8_t> serialize(const GPSPositionData& data) {
                std::vector<std::uint8_t> result(sizeof(GPSPositionData));
                std::memcpy(result.data(), &data, sizeof(GPSPositionData));
                return result;
            }
            
            // Deserialization helper
            static GPSPositionData deserialize(const std::vector<std::uint8_t>& bytes) {
                GPSPositionData data;
                if (bytes.size() >= sizeof(GPSPositionData)) {
                    std::memcpy(&data, bytes.data(), sizeof(GPSPositionData));
                }
                return data;
            }
        };

        INSComponent(Vehicle* vehicle, const unsigned int vehicle_id, const std::string& name, VehicleProt* protocol);

        ~INSComponent() = default;

        void run() override;
        
    protected:
        // Override produce_data_for_response to generate GPS position data
        bool produce_data_for_response(DataTypeId type, std::vector<std::uint8_t>& out_value) override;

    private:
        // Random number generation
        std::mt19937 _gen;
        std::uniform_real_distribution<> _lat_dist;
        std::uniform_real_distribution<> _lon_dist;
        std::uniform_real_distribution<> _alt_dist;
        std::uniform_real_distribution<> _vel_dist;
        std::uniform_real_distribution<> _heading_dist;
        
        // Current GPS position data
        GPSPositionData _current_data;
        
        // Update the simulated GPS position data
        void update_gps_data();
};

/********** INS Component Implementation ***********/
const unsigned int INSComponent::PORT = 104; // Example port for INS

INSComponent::INSComponent(Vehicle* vehicle, const unsigned int vehicle_id, const std::string& name, VehicleProt* protocol) 
    : Component(vehicle, vehicle_id, name, ComponentType::PRODUCER),
    _gen(std::random_device{}()),
    // Define realistic ranges for dummy data
    _lat_dist(-PI_INS/2.0, PI_INS/2.0), // Latitude in radians (-90 to +90 deg)
    _lon_dist(-PI_INS, PI_INS),         // Longitude in radians (-180 to +180 deg)
    _alt_dist(0.0, 500.0),              // Altitude meters
    _vel_dist(0.0, 30.0),               // Velocity m/s
    _heading_dist(0, 2.0 * PI_INS)      // Heading rad (0 to 360 deg)
{
    // Initialize as a producer of GPS_POSITION data
    _produced_data_type = DataTypeId::GPS_POSITION;
    
    // Sets CSV result header
    open_log_file();
    if (_log_file.is_open()) {
        _log_file.seekp(0);
        _log_file << "timestamp_us,latitude_rad,longitude_rad,altitude_m,velocity_mps,heading_rad\n";
        _log_file.flush();
    }
    
    // Initialize with random GPS data
    update_gps_data();

    // Sets own address
    Address addr(_vehicle->address(), PORT);

    // Sets own communicator
    _communicator = new Comms(protocol, addr, ComponentType::PRODUCER, DataTypeId::GPS_POSITION);
    
    db<INSComponent>(INF) << "INS Component initialized as producer of type " 
                         << static_cast<int>(_produced_data_type) << "\n";
}

void INSComponent::run() {
    db<INSComponent>(INF) << "INS component " << getName() << " starting main run loop.\n";
    
    // Send REG_PRODUCER message to Gateway
    Message reg_msg = _communicator->new_message(
        Message::Type::REG_PRODUCER,
        _produced_data_type
    );
    
    // Send to Gateway (port 0)
    Address gateway_addr(_vehicle->address(), 0);
    _communicator->send(reg_msg, gateway_addr);
    
    db<INSComponent>(INF) << "INS sent REG_PRODUCER for type " 
                         << static_cast<int>(_produced_data_type) 
                         << " to Gateway.\n";
    
    // Main loop - update sensor data periodically
    // Note: The actual periodic response sending is handled by producer_response_thread
    while (running()) {
        // Update simulated GPS data
        update_gps_data();
        
        // Log current data
        if (_log_file.is_open()) {
            auto now = std::chrono::high_resolution_clock::now();
            auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                now.time_since_epoch()).count();
                
            _log_file << now_us << ","
                     << std::fixed << std::setprecision(8)  // High precision for GPS coordinates
                     << _current_data.latitude_rad << ","
                     << _current_data.longitude_rad << ","
                     << std::setprecision(3)  // Lower precision for other metrics
                     << _current_data.altitude_m << ","
                     << _current_data.velocity_mps << ","
                     << std::setprecision(5)  // Medium precision for heading
                     << _current_data.heading_rad << "\n";
            _log_file.flush();
        }
        
        // Sleep for a bit - actual response rate is determined by consumer periods
        usleep(100000); // 100ms
    }
    
    db<INSComponent>(INF) << "INS component " << getName() << " exiting main run loop.\n";
}

void INSComponent::update_gps_data() {
    // Generate new simulated data
    _current_data.latitude_rad = _lat_dist(_gen);
    _current_data.longitude_rad = _lon_dist(_gen);
    _current_data.altitude_m = _alt_dist(_gen);
    _current_data.velocity_mps = _vel_dist(_gen);
    _current_data.heading_rad = _heading_dist(_gen);
    
    // Convert to degrees for logging (more human-readable)
    double lat_deg = _current_data.latitude_rad * 180.0 / PI_INS;
    double lon_deg = _current_data.longitude_rad * 180.0 / PI_INS;
    double heading_deg = _current_data.heading_rad * 180.0 / PI_INS;
    
    db<INSComponent>(TRC) << "INS updated position data: lat=" 
                         << std::fixed << std::setprecision(6) << lat_deg << "°, lon="
                         << lon_deg << "°, alt=" << std::setprecision(1)
                         << _current_data.altitude_m << "m, vel="
                         << _current_data.velocity_mps << "m/s, heading="
                         << std::setprecision(1) << heading_deg << "°\n";
}

bool INSComponent::produce_data_for_response(DataTypeId type, std::vector<std::uint8_t>& out_value) {
    // Only respond to requests for our produced data type
    if (type != DataTypeId::GPS_POSITION) {
        return false;
    }
    
    // Serialize the current GPS position data
    out_value = GPSPositionData::serialize(_current_data);
    
    db<INSComponent>(INF) << "INS produced position data with size " << out_value.size() << " bytes\n";
    return true;
}

#endif // INS_COMPONENT_H 