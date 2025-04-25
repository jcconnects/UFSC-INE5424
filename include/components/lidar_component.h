#ifndef LIDAR_COMPONENT_H
#define LIDAR_COMPONENT_H

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

// Assuming ECU2 will have port 2 based on creation order
const unsigned short ECU2_PORT = 2;

class LidarComponent : public Component {
public:
    LidarComponent(Vehicle* vehicle, const std::string& name, TheProtocol* protocol, TheAddress address)
        : Component(vehicle, name, protocol, address),
          _gen(_rd()),
          _coord_dist(-50.0, 50.0),   // Example Lidar range meters
          _intensity_dist(0.1, 1.0), // Example intensity value
          _num_points_dist(100, 500), // Number of points per scan
          _delay_dist(80, 180)      // Milliseconds delay between scans
    {
        open_log_file("lidar_log");
        if (_log_file.is_open()) {
            _log_file.seekp(0);
            _log_file << "timestamp_us,source_vehicle,message_id,event_type,destination_address,payload\n";
            _log_file.flush();
        }

        // Determine the local address for ECU2
        _ecu2_address = TheAddress(address.paddr(), ECU2_PORT);
         db<LidarComponent>(INF) << Component::getName() << " targeting local ECU2 at: " << _ecu2_address << "\n";

        // Define the broadcast address
        _broadcast_address = TheAddress(Ethernet::BROADCAST, 0);
         db<LidarComponent>(INF) << Component::getName() << " targeting broadcast at: " << _broadcast_address << "\n";
    }

    void run() override {
         db<LidarComponent>(INF) << "[" << Component::getName() << "] thread running.\n";
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
                // Limit message size to avoid exceeding buffer - crude check
                if (payload_ss.tellp() > (TheCommunicator::MAX_MESSAGE_SIZE - 200)) {
                     db<LidarComponent>(WRN) << "[" << Component::getName() << "] Approaching max message size, truncating point cloud for msg " << counter << "\n";
                    break;
                }
            }
            payload_ss << "]}";
            std::string payload = payload_ss.str();

            // Construct the full message string
            std::string msg = "[" + Component::getName() + "] Vehicle " + std::to_string(vehicle()->id()) + " message " + std::to_string(counter) + " at " + std::to_string(time_us_system) + ": " + payload;

            // Ensure message isn't too large after adding header
             if (msg.size() >= TheCommunicator::MAX_MESSAGE_SIZE) {
                  db<LidarComponent>(ERR) << "[" << Component::getName() << "] Message " << counter << " exceeds MAX_MESSAGE_SIZE (" << msg.size() << "), skipping send.\n";
                 // TODO: Implement better handling (e.g., fragmentation or reducing points)
                 continue; // Skip this send cycle
             }


            // 1. Send to local ECU2
             db<LidarComponent>(TRC) << "[" << Component::getName() << "] sending msg " << counter << " to ECU2: " << _ecu2_address << "\n";
            int bytes_sent_local = send(_ecu2_address, msg.c_str(), msg.size());
             if (bytes_sent_local > 0) {
                  db<LidarComponent>(INF) << "[" << Component::getName() << "] msg " << counter << " sent locally! (" << bytes_sent_local << " bytes)\n";
                 if (_log_file.is_open()) {
                      _log_file << time_us_system << "," << vehicle()->id() << "," << counter << ",send_local," << _ecu2_address << ",\"" << "PointCloud: " << num_points << " points" << "\"\n"; // Log summary
                      _log_file.flush();
                 }
             } else if(running()) {
                  db<LidarComponent>(ERR) << "[" << Component::getName() << "] failed to send msg " << counter << " locally to " << _ecu2_address << "!\n";
             }


            // 2. Send to broadcast address
             db<LidarComponent>(TRC) << "[" << Component::getName() << "] broadcasting msg " << counter << " to " << _broadcast_address << "\n";
            int bytes_sent_bcast = send(_broadcast_address, msg.c_str(), msg.size());
             if (bytes_sent_bcast > 0) {
                  db<LidarComponent>(INF) << "[" << Component::getName() << "] msg " << counter << " broadcast! (" << bytes_sent_bcast << " bytes)\n";
                 if (_log_file.is_open()) {
                      _log_file << time_us_system << "," << vehicle()->id() << "," << counter << ",send_broadcast," << _broadcast_address << ",\"" << "PointCloud: " << num_points << " points" << "\"\n"; // Log summary
                      _log_file.flush();
                 }
             } else if(running()) {
                  db<LidarComponent>(ERR) << "[" << Component::getName() << "] failed to broadcast msg " << counter << "!\n";
             }

            counter++;

            // Wait for a random delay
            int wait_time_ms = _delay_dist(_gen);
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_time_ms));
        }

         db<LidarComponent>(INF) << "[" << Component::getName() << "] thread terminated.\n";
    }

private:
    TheAddress _ecu2_address;
    TheAddress _broadcast_address;

    // Random number generation
    std::random_device _rd;
    std::mt19937 _gen;
    std::uniform_real_distribution<> _coord_dist;
    std::uniform_real_distribution<> _intensity_dist;
    std::uniform_int_distribution<> _num_points_dist;
    std::uniform_int_distribution<> _delay_dist;
};

#endif // LIDAR_COMPONENT_H 