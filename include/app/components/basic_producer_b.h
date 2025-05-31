#ifndef BASIC_PRODUCER_B_H
#define BASIC_PRODUCER_B_H

#include <random>
#include <cstring>

#include "../../api/framework/agent.h"
#include "../../api/network/bus.h"
#include "../../api/util/debug.h"
#include "../datatypes.h"

class BasicProducerB : public Agent {
    public:
        BasicProducerB(CAN* can, const Message::Origin addr, const std::string& name = "BasicProducerB");
        ~BasicProducerB() = default;

        Agent::Value get(Agent::Unit unit) override;

    private:
        std::random_device _rd;
        std::mt19937 _gen;
        std::uniform_real_distribution<float> _dist;
};

/******** BasicProducerB Implementation *********/
inline BasicProducerB::BasicProducerB(CAN* can, const Message::Origin addr, const std::string& name) 
    : Agent(can, name, static_cast<std::uint32_t>(DataTypes::UNIT_B), CAN::Message::Type::INTEREST, addr),
      _gen(_rd()),
      _dist(200.0f, 300.0f)  // Different range for B
{}

inline Agent::Value BasicProducerB::get(Agent::Unit unit) {
    // Generate a random float value
    float value = _dist(_gen);
    
    db<BasicProducerB>(TRC) << "[BasicProducerB] " << name() 
                           << " generated value: " << value << "\n";

    // Convert float to bytes for the message value
    Agent::Value result(sizeof(float));
    std::memcpy(result.data(), &value, sizeof(float));
    
    return result;
}

#endif // BASIC_PRODUCER_B_H 