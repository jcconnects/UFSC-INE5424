#ifndef BASIC_PRODUCER_A_H
#define BASIC_PRODUCER_A_H

#include <random>
#include <cstring>
#include <cstdint>
#include <string>

#include "../../api/framework/agent.h"
#include "../../api/network/bus.h"
#include "../../api/util/debug.h"
#include "../datatypes.h"

class BasicProducerA : public Agent {
    public:
        BasicProducerA(CAN* can, const Message::Origin addr, const std::string& name = "BasicProducerA");
        ~BasicProducerA() = default;

        Agent::Value get(Agent::Unit unit) override;

    private:
        std::random_device _rd;
        std::mt19937 _gen;
        std::uniform_real_distribution<float> _dist;
};

/******** BasicProducerA Implementation *********/
inline BasicProducerA::BasicProducerA(CAN* can, const Message::Origin addr, const std::string& name) 
    : Agent(can, name, static_cast<std::uint32_t>(DataTypes::UNIT_A), CAN::Message::Type::INTEREST, addr),
      _gen(_rd()),
      _dist(0.0f, 100.0f)
{}

inline Agent::Value BasicProducerA::get(Agent::Unit unit) {
    // Generate a random float value
    float value = _dist(_gen);
    
    db<BasicProducerA>(TRC) << "[BasicProducerA] " << name() 
                           << " generated value: " << value << "\n";

    // Convert float to bytes for the message value
    Agent::Value result(sizeof(float));
    std::memcpy(result.data(), &value, sizeof(float));
    
    return result;
}

#endif // BASIC_PRODUCER_A_H 