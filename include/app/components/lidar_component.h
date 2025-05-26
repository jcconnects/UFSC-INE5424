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

#include "api/framework/agent.h"
#include "api/network/bus.h"
#include "../api/util/debug.h"

class LidarComponent : public Agent {
    public:
        LidarComponent(CAN* can, const std::string name = "LidarComponent");
        ~LidarComponent() override = default;

        Agent::Value get(Agent::Unit unit) override;

    private:
        // Random number generation
        std::random_device _rd;
        std::mt19937 _gen;
        std::uniform_real_distribution<> _coord_dist;
        std::uniform_real_distribution<> _intensity_dist;
        std::uniform_int_distribution<> _num_points_dist;
        std::uniform_int_distribution<> _delay_dist;
        unsigned int _counter;
};

/********** Lidar Component Implementation ***********/
const unsigned int LidarComponent::PORT = static_cast<unsigned int>(Vehicle::Ports::LIDAR);

LidarComponent::LidarComponent(CAN* can, const std::string& name) : Agent(can, name),
    _gen(_rd()),
    _coord_dist(-50.0, 50.0),   // Example Lidar range meters
    _intensity_dist(0.1, 1.0), // Example intensity value
    _num_points_dist(20, 50), // Number of points per scan
    _delay_dist(80, 180),      // Milliseconds delay between scans
    _counter(0)  // Initialize counter for message numbering
{
    add_produced_type(DataTypes::EXTERNAL_POINT_CLOUD_XYZ, CAN::Message::Type::INSTEREST);
}

Agent::Value get(Agent::Unit unit) {
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
    std::string msg = "[" + Component::getName() + "] Vehicle " + std::to_string(vehicle()->id()) + " message " + std::to_string(_counter) + " at " + std::to_string(time_us_system) + ": " + payload;

    _counter++;

    return Agent::Value(msg.begin(), msg.end());
}

#endif // LIDAR_COMPONENT_H 