#ifndef BASIC_CONSUMER_A_H
#define BASIC_CONSUMER_A_H

#include <cstdint>
#include <string>

#include "../../api/framework/agent.h"
#include "../../api/network/bus.h"
#include "../../api/util/debug.h"
#include "../datatypes.h"

class BasicConsumerA : public Agent {
    public:
        BasicConsumerA(CAN* can, const Message::Origin addr, const std::string& name = "BasicConsumerA");
        ~BasicConsumerA() = default;

        Agent::Value get(Agent::Unit unit) override;
        void handle_response(Message* msg) override;

    private:
        float _last_received_value;
};

/******** BasicConsumerA Implementation *********/
inline BasicConsumerA::BasicConsumerA(CAN* can, const Message::Origin addr, const std::string& name) 
    : Agent(can, name, static_cast<std::uint32_t>(DataTypes::UNIT_A), CAN::Message::Type::RESPONSE, addr),
      _last_received_value(0.0f)
{}

inline Agent::Value BasicConsumerA::get(Agent::Unit unit) {
    // This consumer doesn't produce data, return empty value
    return Agent::Value();
}

inline void BasicConsumerA::handle_response(Message* msg) {
    const std::uint8_t* received_value = msg->value();
    _last_received_value = *reinterpret_cast<const float*>(received_value);
    
    db<BasicConsumerA>(INF) << "[BasicConsumerA] " << name() 
                            << " received RESPONSE message with value: " << _last_received_value 
                            << " and value size: " << msg->value_size() << "\n";
    
}

#endif // BASIC_CONSUMER_A_H 