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

#include "api/framework/agent.h"
#include "api/network/bus.h"
#include "../api/util/debug.h"
#include "app/datatypes.h"


class CameraComponent : public Agent {
    public:
        CameraComponent(CAN* can, const std::string name = "CameraComponent");
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
CameraComponent::CameraComponent(CAN* can, const std::string& name) : Agent(can, name),
    _gen(_rd()),
    _coord_dist(0.0, 1920.0), // Example camera resolution width
    _size_dist(50.0, 300.0),   // Example bounding box size
    _label_dist(0, _labels.size() - 1),
    _delay_dist(50, 150) // Milliseconds delay between sends
{   
    add_produced_type(DataTypes::EXTERNAL_PIXEL_MATRIX, CAN::Message::Type::INSTEREST);
}

Agent::Value get(Agent::Unit unit) {
    switch (unit) {
        case static_cast<std::uint32_t>(DataTypes::RGB_IMAGE):
            // TODO
            break;
        case static_cast<std::uint32_t>(DataTypes::VIDEO_STREAM):
            // TODO
            break;
        case static_cast<std::uint32_t>(DataTypes::PIXEL_MATRIX):
            // TODO
            break;
        case static_cast<std::uint32_t>(DataTypes::CAMERA_METADATA):
            // TODO
            break;
        default:
            Agent::Value val;
            return val;
    }
}



#endif // CAMERA_COMPONENT_H 