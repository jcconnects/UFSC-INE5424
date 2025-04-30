#ifndef INS_COMPONENT_H
#define INS_COMPONENT_H

#include "component.h"
#include "vehicle.h"
#include "debug.h"
#include "ethernet.h"
#include <chrono>
#include <random>
#include <unistd.h>
#include <thread>
#include <string>
#include <sstream>
#include <vector>
#include <iomanip> // For std::fixed, std::setprecision
#include <cmath>   // For M_PI if needed, though using numeric literal is safer

// Constants
constexpr double PI_INS = 3.14159265358979323846;
constexpr double G_TO_MS2_INS = 9.80665;
constexpr double DEG_TO_RAD_INS = PI_INS / 180.0;

// Assuming ECU1 will have port 0 based on creation order
// Note: If camera_component.h is also included, this might be a redefinition.
// Consider moving constants to a shared header or using include guards carefully.
extern const unsigned short ECU1_PORT; // Use extern if defined elsewhere

class INSComponent : public Component {
public:
    INSComponent(Vehicle* vehicle, const std::string& name, TheProtocol* protocol, TheAddress address)
        : Component(vehicle, name, protocol, address),
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
    {
        open_log_file("ins_log");
        if (_log_file.is_open()) {
            _log_file.seekp(0);
            _log_file << "timestamp_us,source_vehicle,message_id,event_type,destination_address,latitude_rad,longitude_rad,altitude_m,velocity_mps,accel_x_mps2,accel_y_mps2,accel_z_mps2,gyro_x_radps,gyro_y_radps,gyro_z_radps,heading_rad\n";
            _log_file.flush();
        }

        // Determine the local address for ECU1
        _ecu1_address = TheAddress(address.paddr(), ECU1_PORT);
         db<INSComponent>(INF) << Component::getName() << " targeting local ECU1 at: " << _ecu1_address << "\n";

        // Define the broadcast address
        _broadcast_address = TheAddress(Ethernet::BROADCAST, ECU1_PORT);
         db<INSComponent>(INF) << Component::getName() << " targeting broadcast at: " << _broadcast_address << "\n";
    }

    void run() override {
         db<INSComponent>(INF) << "[" << Component::getName() << "] thread running.\n";
        int counter = 1;

        while (running()) {
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
            std::string msg = "[" + Component::getName() + "] Vehicle " + std::to_string(vehicle()->id()) + " message " + std::to_string(counter) + " at " + std::to_string(time_us_system) + ": " + payload;

             // Ensure message isn't too large
              if (msg.size() >= TheCommunicator::MAX_MESSAGE_SIZE) {
                   db<INSComponent>(ERR) << "[" << Component::getName() << "] Message " << counter << " exceeds MAX_MESSAGE_SIZE (" << msg.size() << "), skipping send.\n";
                  continue;
              }

            // 1. Send to local ECU1
             db<INSComponent>(TRC) << "[" << Component::getName() << "] sending msg " << counter << " to ECU1: " << _ecu1_address << "\n";
            int bytes_sent_local = send(_ecu1_address, msg.c_str(), msg.size());
             if (bytes_sent_local > 0) {
                  db<INSComponent>(INF) << "[" << Component::getName() << "] msg " << counter << " sent locally! (" << bytes_sent_local << " bytes)\n";
                 if (_log_file.is_open()) {
                      _log_file << time_us_system << "," << vehicle()->id() << "," << counter << ",send_local," << _ecu1_address << ","
                                << std::fixed << std::setprecision(8) << lat << "," << lon << "," << std::setprecision(3) << alt << "," << vel << ","
                                << accel_x << "," << accel_y << "," << accel_z << ","
                                << std::setprecision(5) << gyro_x << "," << gyro_y << "," << gyro_z << "," << heading << "\n";
                      _log_file.flush();
                 }
             } else if(running()) {
                  db<INSComponent>(ERR) << "[" << Component::getName() << "] failed to send msg " << counter << " locally to " << _ecu1_address << "!\n";
             }

            // 2. Send to broadcast address
             db<INSComponent>(TRC) << "[" << Component::getName() << "] broadcasting msg " << counter << " to " << _broadcast_address << "\n";
            int bytes_sent_bcast = send(_broadcast_address, msg.c_str(), msg.size());
            if (bytes_sent_bcast > 0) {
                db<INSComponent>(INF) << "[" << Component::getName() << "] msg " << counter << " broadcast! (" << bytes_sent_bcast << " bytes)\n";
                 if (_log_file.is_open()) {
                     // Could log broadcast sends too, but might be redundant with local send log
                      // _log_file << time_us_system << "," << vehicle()->id() << "," << counter << ",send_broadcast," << _broadcast_address << ",...";
                      // _log_file.flush();
                 }
             } else if(running()) {
                  db<INSComponent>(ERR) << "[" << Component::getName() << "] failed to broadcast msg " << counter << "!\n";
             }


            counter++;

            // Wait for a delay
            int wait_time_ms = _delay_dist(_gen);
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_time_ms));
        }

         db<INSComponent>(INF) << "[" << Component::getName() << "] thread terminated.\n";
    }

private:
    TheAddress _ecu1_address;
    TheAddress _broadcast_address;

    // Random number generation
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

#endif // INS_COMPONENT_H 