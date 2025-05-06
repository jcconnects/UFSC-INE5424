#ifndef CAMERA_COMPONENT_H
#define CAMERA_COMPONENT_H

#include <chrono>
#include <random>
#include <unistd.h> // For usleep
#include <thread>   // For std::this_thread::sleep_for
#include <string>
#include <sstream>  // For string formatting
#include <vector>
#include <iomanip> // For std::fixed, std::setprecision

#include "component.h"
#include "debug.h"


class CameraComponent : public Component {
    public:
        static const unsigned int PORT;

        CameraComponent(Vehicle* vehicle, const unsigned int vehicle_id, const std::string& name, VehicleProt* protocol);

        ~CameraComponent() override = default;

        void run() override;

    private:
        // Random number generation for dummy data and delay
        std::random_device _rd;
        std::mt19937 _gen;
        std::uniform_real_distribution<> _coord_dist;
        std::uniform_real_distribution<> _size_dist;
        std::uniform_int_distribution<> _label_dist;
        std::uniform_int_distribution<> _delay_dist;

        const std::vector<std::string> _labels = {"car", "pedestrian", "bicycle", "traffic_light"};
};

/******** Camera Component Implementation *********/
const unsigned int CameraComponent::PORT = static_cast<unsigned int>(Vehicle::Ports::CAMERA);

CameraComponent::CameraComponent(Vehicle* vehicle, const unsigned int vehicle_id, const std::string& name, VehicleProt* protocol) : Component(vehicle, vehicle_id, name),
    _gen(_rd()),
    _coord_dist(0.0, 1920.0), // Example camera resolution width
    _size_dist(50.0, 300.0),   // Example bounding box size
    _label_dist(0, _labels.size() - 1),
    _delay_dist(50, 150) // Milliseconds delay between sends
{
    // Sets CSV result header
    open_log_file();
    if (_log_file.is_open()) {
        _log_file.seekp(0);
        _log_file << "timestamp_us,source_vehicle,message_id,event_type,destination_address,payload\n";
        _log_file.flush();
    }

    // Sets own address
    Address addr(_vehicle->address(), CameraComponent::PORT);

    // Sets own communicator
    _communicator = new Comms(protocol, addr);
}

void CameraComponent::run() {
    db<CameraComponent>(INF) << "[CameraComponent] " << Component::getName() << " thread running.\n";
    
    // Message counter
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
        std::string msg = "[" + Component::getName() + "] Vehicle " + std::to_string(vehicle()->id()) + " message " + std::to_string(counter) + " at " + std::to_string(time_us_system) + ": " + payload;

        // 1. Send to local ECU1
        Address ecu1_address(_vehicle->address(), static_cast<unsigned int>(Vehicle::Ports::ECU1));
        db<CameraComponent>(INF) << "[CameraComponent] " << Component::getName() << " sending message " << counter << " to ECU1: " << ecu1_address.to_string() << "\n";

        
        int bytes_sent_local = send(msg.c_str(), msg.size(), ecu1_address);

        if (bytes_sent_local > 0) {
            db<CameraComponent>(INF) << "[CameraComponent] " << Component::getName() << " message " << counter << " sent locally! (" << bytes_sent_local << " bytes)\n";

            // File is already open (on constructor)
            _log_file << time_us_system << "," << vehicle()->id() << "," << counter << ",send_local," << ecu1_address.to_string() << ",\"" << payload << "\"\n";
            _log_file.flush();
        
        } else if (running()){ // Only log error if still supposed to be running
            db<CameraComponent>(ERR) << "[CameraComponent] " << Component::getName() << " failed to send message " << counter << " locally to " << ecu1_address.to_string() << "!\n";
        }

        // 2. Send to broadcast address
        db<CameraComponent>(INF) << "[CameraComponent] " << Component::getName() << "] broadcasting message " << counter << ".\n";
        int bytes_sent_bcast = send(msg.c_str(), msg.size());

        if (bytes_sent_bcast > 0) {
            db<CameraComponent>(INF) << "[CameraComponent] " << Component::getName() << " message " << counter << " broadcasted! (" << bytes_sent_bcast << " bytes)\n";

            // File is already open (on constructor)
            _log_file << time_us_system << "," << vehicle()->id() << "," << counter << ",send_broadcast," << Address::BROADCAST.to_string() << ",\"" << payload << "\"\n";
            _log_file.flush();

        } else if (running()) {
            db<CameraComponent>(ERR) << "[CameraComponent] " << Component::getName() << " failed to broadcast message " << counter << "!\n";
        }

        counter++;

        // Wait for a random delay
        int wait_time_ms = _delay_dist(_gen);
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_time_ms));
    }

    db<CameraComponent>(INF) << "[" << Component::getName() << "] thread terminated.\n";
}
#endif // CAMERA_COMPONENT_H 