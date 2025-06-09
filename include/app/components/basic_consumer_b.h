#ifndef BASIC_CONSUMER_B_H
#define BASIC_CONSUMER_B_H

#include <cstdint>
#include <string>

#include "../../api/framework/agent.h"
#include "../../api/network/bus.h"
#include "../../api/util/debug.h"
#include "../datatypes.h"

class BasicConsumerB : public Agent {
    public:
        BasicConsumerB(CAN* can, const Message::Origin addr, const std::string& name = "BasicConsumerB");
        ~BasicConsumerB() = default;

        Agent::Value get(Agent::Unit unit) override;
        void handle_response(Message* msg) override;
        
        void start_consuming(Microseconds period = Microseconds(1000000));

    private:
        float _last_received_value;
};

/******** BasicConsumerB Implementation *********/
inline BasicConsumerB::BasicConsumerB(CAN* can, const Message::Origin addr, const std::string& name) 
    : Agent(can, name, static_cast<std::uint32_t>(DataTypes::UNIT_B), CAN::Message::Type::RESPONSE, addr),
      _last_received_value(0.0f)
{}

inline Agent::Value BasicConsumerB::get(Agent::Unit unit) {
    // This consumer doesn't produce data, return empty value
    return Agent::Value();
}

inline void BasicConsumerB::handle_response(Message* msg) {
    const std::uint8_t* received_value = msg->value();
    _last_received_value = *reinterpret_cast<const float*>(received_value);
    
    db<BasicConsumerB>(INF) << "[BasicConsumerB] " << name() 
                            << " received RESPONSE message with value: " << _last_received_value 
                            << " and value size: " << msg->value_size() << "\n";
}

/**
 * @brief Start consuming data with the specified period
 * 
 * @param period The desired response period from producers (default: 1 second)
 */
inline void BasicConsumerB::start_consuming(Microseconds period) {
    start_periodic_interest(static_cast<std::uint32_t>(DataTypes::UNIT_B), period);
}

#endif // BASIC_CONSUMER_B_H 