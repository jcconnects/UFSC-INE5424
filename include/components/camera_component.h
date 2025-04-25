#ifndef CAMERA_COMPONENT_H
#define CAMERA_COMPONENT_H

#include "component.h"
#include "vehicle.h" // Access vehicle ID
#include "INFug.h"
#include "ethernet.h" // For broadcast address
#include <chrono>
#include <random>
#include <unistd.h> // For usleep
#include <thread>   // For std::this_thread::sleep_for
#include <string>
#include <sstream>  // For string formatting
#include <vector>
#include <iomanip> // For std::fixed, std::setprecision

// Forward declaration if necessary, or include the relevant header
// Assuming ECU1 will have port 0 based on creation order
const unsigned short ECU1_PORT = 0;

class CameraComponent : public Component {
public:
    CameraComponent(Vehicle* vehicle, const std::string& name, TheProtocol* protocol, TheAddress address)
        : Component(vehicle, name, protocol, address),
          _gen(_rd()),
          _coord_dist(0.0, 1920.0), // Example camera resolution width
          _size_dist(50.0, 300.0),   // Example bounding box size
          _label_dist(0, _labels.size() - 1),
          _delay_dist(50, 150) // Milliseconds delay between sends
    {
        open_log_file("camera_log");
        if (_log_file.is_open()) {
            _log_file.seekp(0);
            _log_file << "timestamp_us,source_vehicle,message_id,event_type,destination_address,payload\n";
            _log_file.flush();
        }

        // Determine the local address for ECU1
        // We assume the base address is the vehicle's MAC and ECU1 gets port ECU1_PORT
        _ecu1_address = TheAddress(address.PADDR(), ECU1_PORT);
        db<CameraComponent>(INF) << name() << " targeting local ECU1 at: " << _ecu1_address << "\n";

        // Define the broadcast address
        _broadcast_address = TheAddress(Ethernet::BROADCAST, 0); // Target port 0 for broadcast for simplicity
         db<CameraComponent>(INF) << name() << " targeting broadcast at: " << _broadcast_address << "\n";

    }

    void run() override {
         db<CameraComponent>(INF) << "[" << name() << "] thread running.\n";
        int counter = 1;

        while (running()) {
            auto now_system = std::chrono::system_clock::now();
            auto time_us_system = std::chrono::duration_cast<std::chrono::microseconds>(now_system.time_since_epoch()).count();

            // Generate dummy detection data
            std::stringstream payload_ss;
            payload_ss << std::fixed << std::setprecision(2);
            int num_detections = _label_dist(_gen) + 1; // Generate 1 to N detections
            payload_ss << "Detections: [";
            for (int i = 0; i < num_detections; ++i) {
                double x = _coord_dist(_gen);
                double y = _coord_dist(_gen);
                double w = _size_dist(_gen);
                double h = _size_dist(_gen);
                std::string label = _labels[_label_dist(_gen)];
                payload_ss << (i > 0 ? ", " : "") << "{label: " << label << ", bbox: [" << x << ", " << y << ", " << w << ", " << h << "]}";
            }
            payload_ss << "]";
            std::string payload = payload_ss.str();

            // Construct the full message string including metadata
            std::string msg = "[" + name() + "] Vehicle " + std::to_string(vehicle()->id()) + " message " + std::to_string(counter) + " at " + std::to_string(time_us_system) + ": " + payload;

            // 1. Send to local ECU1
             db<CameraComponent>(TRC) << "[" << name() << "] sending msg " << counter << " to ECU1: " << _ecu1_address << "\n";
            int bytes_sent_local = send(_ecu1_address, msg.c_str(), msg.size());
            if (bytes_sent_local > 0) {
                 db<CameraComponent>(INF) << "[" << name() << "] msg " << counter << " sent locally! (" << bytes_sent_local << " bytes)\n";
                 if (_log_file.is_open()) {
                     _log_file << time_us_system << "," << vehicle()->id() << "," << counter << ",send_local," << _ecu1_address << ",\"" << payload << "\"\n";
                     _log_file.flush();
                 }
            } else if (running()){ // Only log error if still supposed to be running
                 db<CameraComponent>(ERR) << "[" << name() << "] failed to send msg " << counter << " locally to " << _ecu1_address << "!\n";
            }

            // 2. Send to broadcast address
             db<CameraComponent>(TRC) << "[" << name() << "] broadcasting msg " << counter << " to " << _broadcast_address << "\n";
            int bytes_sent_bcast = send(_broadcast_address, msg.c_str(), msg.size());
             if (bytes_sent_bcast > 0) {
                  db<CameraComponent>(INF) << "[" << name() << "] msg " << counter << " broadcast! (" << bytes_sent_bcast << " bytes)\n";
                 if (_log_file.is_open()) {
                      _log_file << time_us_system << "," << vehicle()->id() << "," << counter << ",send_broadcast," << _broadcast_address << ",\"" << payload << "\"\n";
                      _log_file.flush();
                 }
             } else if (running()) {
                  db<CameraComponent>(ERR) << "[" << name() << "] failed to broadcast msg " << counter << "!\n";
             }

            counter++;

            // Wait for a random delay
            int wait_time_ms = _delay_dist(_gen);
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_time_ms));
        }

         db<CameraComponent>(INF) << "[" << name() << "] thread terminated.\n";
    }

private:
    TheAddress _ecu1_address;
    TheAddress _broadcast_address;

    // Random number generation for dummy data and delay
    std::random_device _rd;
    std::mt19937 _gen;
    std::uniform_real_distribution<> _coord_dist;
    std::uniform_real_distribution<> _size_dist;
    std::uniform_int_distribution<> _label_dist;
    std::uniform_int_distribution<> _delay_dist;

    const std::vector<std::string> _labels = {"car", "pedestrian", "bicycle", "traffic_light"};
};

#endif // CAMERA_COMPONENT_H 