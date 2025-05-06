#ifndef LIDAR_COMPONENT_H
#define LIDAR_COMPONENT_H


#include <chrono>
#include <random>
#include <unistd.h>
#include <thread>
#include <string>
#include <sstream>
#include <vector>
#include <iomanip> // For std::fixed, std::setprecision

#include "component.h"
#include "debug.h"

class LidarComponent : public Component {
    public:
        static const unsigned int PORT;

        LidarComponent(Vehicle* vehicle, const unsigned int vehicle_id, const std::string& name, VehicleProt* protocol);

        ~LidarComponent() override = default;

        void run() override;

    private:
        // Random number generation
        std::random_device _rd;
        std::mt19937 _gen;
        std::uniform_real_distribution<> _coord_dist;
        std::uniform_real_distribution<> _intensity_dist;
        std::uniform_int_distribution<> _num_points_dist;
        std::uniform_int_distribution<> _delay_dist;
};

/********** Lidar Component Implementation ***********/
const unsigned int LidarComponent::PORT = static_cast<unsigned int>(Vehicle::Ports::LIDAR);

LidarComponent::LidarComponent(Vehicle* vehicle, const unsigned int vehicle_id, const std::string& name, VehicleProt* protocol) : Component(vehicle, vehicle_id, name),
    _gen(_rd()),
    _coord_dist(-50.0, 50.0),   // Example Lidar range meters
    _intensity_dist(0.1, 1.0), // Example intensity value
    _num_points_dist(20, 50), // Number of points per scan
    _delay_dist(80, 180)      // Milliseconds delay between scans
{
    // Sets CSV result header
    open_log_file();
    if (_log_file.is_open()) {
        _log_file.seekp(0);
        _log_file << "timestamp_us,source_vehicle,message_id,event_type,destination_address,payload\n";
        _log_file.flush();
    }

    // Sets own address
    Address addr(_vehicle->address(), LidarComponent::PORT);

    // Sets own communicator
    _communicator = new Comms(protocol, addr);
}

void LidarComponent::run() {
    db<LidarComponent>(INF) << "[LidarComponent] " << Component::getName() << " thread running.\n";

    // Message counter
    int counter = 1;

    while (running()) {
        auto now_system = std::chrono::system_clock::now();
        auto time_us_system = std::chrono::duration_cast<std::chrono::microseconds>(now_system.time_since_epoch()).count();

        // Generate dummy point cloud data (simplified)
        std::stringstream payload_ss;
        payload_ss << std::fixed << std::setprecision(3); // Use more precision for point clouds
        int num_points = _num_points_dist(_gen);
        payload_ss << "PointCloud: {num_points: " << num_points << ", points: [";
        for (int i = 0; i < num_points; ++i) {
            double x = _coord_dist(_gen);
            double y = _coord_dist(_gen);
            double z = _coord_dist(_gen) / 5.0; // Smaller Z range typically
            double intensity = _intensity_dist(_gen);
            payload_ss << (i > 0 ? ", " : "") << "[" << x << ", " << y << ", " << z << ", " << intensity << "]";
        }

        payload_ss << "]}";
        std::string payload = payload_ss.str();

        // Construct the full message string
        std::string msg = "[" + Component::getName() + "] Vehicle " + std::to_string(vehicle()->id()) + " message " + std::to_string(counter) + " at " + std::to_string(time_us_system) + ": " + payload;

        // 1. Send to local ECU2
        Address ecu2_address(_vehicle->address(), static_cast<unsigned int>(Vehicle::Ports::ECU2));

        db<LidarComponent>(INF) << "[LidarComponent] " << Component::getName() << " sending message " << counter << " to ECU2: " << ecu2_address.to_string() << "\n";
        int bytes_sent_local = send(msg.c_str(), msg.size(), ecu2_address);

        if (bytes_sent_local > 0) {
            db<LidarComponent>(INF) << "[LidarComponent] " << Component::getName() << " message " << counter << " sent locally! (" << bytes_sent_local << " bytes)\n";
            _log_file << time_us_system << "," << vehicle()->id() << "," << counter << ",send_local," << ecu2_address.to_string() << ",\"" << "PointCloud: " << num_points << " points" << "\"\n";
            _log_file.flush();

        } else if(running()) {
            db<LidarComponent>(ERR) << "[LidarComponent] " << Component::getName() << " failed to send message " << counter << " locally to " << ecu2_address.to_string() << "!\n";
        }


        // 2. Send to broadcast address
        db<LidarComponent>(INF) << "[LidarComponent] " << Component::getName() << " broadcasting message " << counter << ".\n";
        int bytes_sent_bcast = send(msg.c_str(), msg.size());

        if (bytes_sent_bcast > 0) {
            db<LidarComponent>(INF) << "[" << Component::getName() << "] msg " << counter << " broadcast! (" << bytes_sent_bcast << " bytes)\n";
            _log_file << time_us_system << "," << vehicle()->id() << "," << counter << ",send_broadcast," << Address::BROADCAST.to_string() << ",\"" << "PointCloud: " << num_points << " points" << "\"\n"; // Log summary
            _log_file.flush();
        } else if(running()) {
            db<LidarComponent>(ERR) << "[LidarComponent] " << Component::getName() << " failed to broadcast message " << counter << "!\n";
        }

        counter++;

        // Wait for a random delay
        int wait_time_ms = _delay_dist(_gen);
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_time_ms));
    }

    db<LidarComponent>(INF) << "[LidarComponent] " << Component::getName() << " thread terminated.\n";
}

#endif // LIDAR_COMPONENT_H 