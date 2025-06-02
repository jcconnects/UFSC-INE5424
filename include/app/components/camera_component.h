#ifndef CAMERA_COMPONENT_H
#define CAMERA_COMPONENT_H

#include <chrono>
#include <random>
#include <unistd.h>
#include <string>
#include <sstream>
#include <vector>
#include <iomanip>
#include <cstdint>

#include "api/framework/agent.h"
#include "api/network/bus.h"
#include "../api/util/debug.h"
#include "app/datatypes.h"


class CameraComponent : public Agent {
    public:
        CameraComponent(CAN* can, const Message::Origin addr, const std::string& name = "CameraComponent");
        ~CameraComponent() = default;

        Agent::Value get(Agent::Unit unit) override;

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
inline CameraComponent::CameraComponent(CAN* can, const Message::Origin addr, const std::string& name) : Agent(can, name, static_cast<std::uint32_t>(DataTypes::EXTERNAL_PIXEL_MATRIX), CAN::Message::Type::INTEREST, addr),
    _gen(_rd()),
    _coord_dist(0.0, 1920.0), // Example camera resolution width
    _size_dist(50.0, 300.0),   // Example bounding box size
    _label_dist(0, _labels.size() - 1),
    _delay_dist(50, 150) // Milliseconds delay between sends
{}

inline Agent::Value CameraComponent::get(Agent::Unit unit) {
    auto now_system = std::chrono::system_clock::now();
    auto time_us_system = std::chrono::duration_cast<std::chrono::microseconds>(now_system.time_since_epoch()).count();

    std::stringstream payload_ss;
    
    switch (unit) {
        case static_cast<std::uint32_t>(DataTypes::RGB_IMAGE):
        case static_cast<std::uint32_t>(DataTypes::EXTERNAL_RGB_IMAGE):
            payload_ss << "RGB_Image: {width: 1920, height: 1080, format: RGB24}";
            break;
        case static_cast<std::uint32_t>(DataTypes::VIDEO_STREAM):
        case static_cast<std::uint32_t>(DataTypes::EXTERNAL_VIDEO_STREAM):
            payload_ss << "Video_Stream: {fps: 30, codec: H264, bitrate: 5000}";
            break;
        case static_cast<std::uint32_t>(DataTypes::PIXEL_MATRIX):
        case static_cast<std::uint32_t>(DataTypes::EXTERNAL_PIXEL_MATRIX): {
            // Generate dummy object detection data
            payload_ss << "Objects: [";
            int num_objects = _label_dist(_gen) % 3 + 1; // 1-3 objects
            for (int i = 0; i < num_objects; ++i) {
                double x = _coord_dist(_gen);
                double y = _coord_dist(_gen) * 0.5625; // 16:9 aspect ratio
                double w = _size_dist(_gen);
                double h = _size_dist(_gen);
                std::string label = _labels[_label_dist(_gen)];
                
                payload_ss << (i > 0 ? ", " : "") << "{label: \"" << label 
                          << "\", bbox: [" << std::fixed << std::setprecision(1) 
                          << x << ", " << y << ", " << w << ", " << h << "]}";
            }
            payload_ss << "]";
            break;
        }
        case static_cast<std::uint32_t>(DataTypes::CAMERA_METADATA):
        case static_cast<std::uint32_t>(DataTypes::EXTERNAL_CAMERA_METADATA):
            payload_ss << "Camera_Meta: {exposure: " << (_coord_dist(_gen) / 1000.0) 
                      << ", iso: " << (100 + _label_dist(_gen) * 100) 
                      << ", focus: " << (_size_dist(_gen) / 100.0) << "}";
            break;
        default:
            payload_ss << "Unknown_Camera_Data";
            break;
    }
    
    std::string payload = payload_ss.str();
    std::string msg = "[" + name() + "] " + payload + " at " + std::to_string(time_us_system);

    db<CameraComponent>(TRC) << "[CameraComponent] " << name() 
                            << " generated message: " << msg << "\n";

    return Agent::Value(msg.begin(), msg.end());
}



#endif // CAMERA_COMPONENT_H 