#ifndef INS_COMPONENT_H
#define INS_COMPONENT_H

#include <chrono>
#include <random>
#include <unistd.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <ios>

#include "api/framework/agent.h"
#include "api/network/bus.h"
#include "../api/util/debug.h"

// Constants
constexpr double PI_INS = 3.14159265358979323846;
constexpr double G_TO_MS2_INS = 9.80665;
constexpr double DEG_TO_RAD_INS = PI_INS / 180.0;


class INSComponent : public Agent {
    public:
        INSComponent(CAN* can, Message::Origin addr, const std::string& name = "INSComponent");
        ~INSComponent() override = default;

        Agent::Value get(Agent::Unit unit) override;

    private:
        // Random number generation
        unsigned int _counter; // Message _counter
        std::random_device _rd;
        std::mt19937 _gen;
        std::uniform_real_distribution<> _lat_dist;
        std::uniform_real_distribution<> _lon_dist;
        std::uniform_real_distribution<> _alt_dist;
        std::uniform_real_distribution<> _vel_dist;
        std::uniform_real_distribution<> _accel_dist;
        std::uniform_real_distribution<> _gyro_dist;
        std::uniform_real_distribution<> _heading_dist;
        std::uniform_int_distribution<> _delay_dist;
};

INSComponent::INSComponent(CAN* can, Message::Origin addr, const std::string& name) : Agent(can, name, static_cast<std::uint32_t>(DataTypes::EXTERNAL_INERTIAL_POSITION), CAN::Message::Type::INTEREST, addr),
    _counter(0),
    _gen(_rd()),
    // Define realistic ranges for dummy data
    _lat_dist(-PI_INS/2.0, PI_INS/2.0), // Latitude in radians (-90 to +90 deg)
    _lon_dist(-PI_INS, PI_INS),          // Longitude in radians (-180 to +180 deg)
    _alt_dist(0.0, 500.0),       // Altitude meters
    _vel_dist(0.0, 30.0),        // Velocity m/s
    _accel_dist(-2.0 * G_TO_MS2_INS, 2.0 * G_TO_MS2_INS), // Acceleration m/s^2 (+/- 2g)
    _gyro_dist(-PI_INS, PI_INS),        // Gyro rad/s (+/- 180 deg/s)
    _heading_dist(0, 2.0 * PI_INS),  // Heading rad (0 to 360 deg)
    _delay_dist(90, 110)        // Milliseconds delay (INS typically ~10Hz)
{}

inline Agent::Value INSComponent::get(Agent::Unit unit) {
    auto now_system = std::chrono::system_clock::now();
    auto time_us_system = std::chrono::duration_cast<std::chrono::microseconds>(now_system.time_since_epoch()).count();

    // Generate dummy INS data
    double lat = _lat_dist(_gen);
    double lon = _lon_dist(_gen);
    double alt = _alt_dist(_gen);
    double vel = _vel_dist(_gen);
    double accel_x = _accel_dist(_gen);
    double accel_y = _accel_dist(_gen);
    double accel_z = _accel_dist(_gen);
    double gyro_x = _gyro_dist(_gen);
    double gyro_y = _gyro_dist(_gen);
    double gyro_z = _gyro_dist(_gen);
    double heading = _heading_dist(_gen);

    std::stringstream payload_ss;
    payload_ss << std::fixed << std::setprecision(8) // High precision for GPS/IMU
            << "INSData: {"
            << "Lat: " << lat << ", Lon: " << lon << ", Alt: " << alt
            << ", Vel: " << std::setprecision(3) << vel // Lower precision for others
            << ", Accel: [" << accel_x << ", " << accel_y << ", " << accel_z << "]"
            << ", Gyro: [" << std::setprecision(5) << gyro_x << ", " << gyro_y << ", " << gyro_z << "]"
            << ", Heading: " << heading
            << "}";
    std::string payload = payload_ss.str();

    // Construct the full message string
    std::string msg = "[" + name() + "] message " + std::to_string(_counter) + " at " + std::to_string(time_us_system) + ": " + payload;

    db<INSComponent>(TRC) << "[INSComponent] " << name() 
                          << " generated message: " << msg << "\n";

    return Agent::Value(msg.begin(), msg.end());
}

#endif // INS_COMPONENT_H 