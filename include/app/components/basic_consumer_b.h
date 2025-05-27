#ifndef BASIC_CONSUMER_B_H
#define BASIC_CONSUMER_B_H

#include <cstring>

#include "api/framework/agent.h"
#include "api/network/bus.h"
#include "../api/util/debug.h"
#include "app/datatypes.h"

class BasicConsumerB : public Agent {
    public:
        BasicConsumerB(CAN* can, const Message::Origin addr, const std::string& name = "BasicConsumerB");
        ~BasicConsumerB() = default;

        Agent::Value get(Agent::Unit unit) override;
        void handle_response(Message* msg) override;

    private:
        float _last_received_value;
};

/******** BasicConsumerB Implementation *********/
BasicConsumerB::BasicConsumerB(CAN* can, const Message::Origin addr, const std::string& name) 
    : Agent(can, name, static_cast<std::uint32_t>(DataTypes::UNIT_B), CAN::Message::Type::RESPONSE, addr),
      _last_received_value(0.0f)
{}

Agent::Value BasicConsumerB::get(Agent::Unit unit) {
    // This consumer doesn't produce data, return empty value
    return Agent::Value();
}

void BasicConsumerB::handle_response(Message* msg) {
    if (msg->value_size() == sizeof(float)) {
        float received_value;
        std::memcpy(&received_value, msg->value(), sizeof(float));
        
        // Add 1 to the received value
        _last_received_value = received_value + 1.0f;
        
        db<BasicConsumerB>(INF) << "[BasicConsumerB] " << name() 
                               << " received value: " << received_value 
                               << ", processed value: " << _last_received_value << "\n";
    } else {
        db<BasicConsumerB>(WRN) << "[BasicConsumerB] " << name() 
                               << " received message with unexpected size: " << msg->value_size() << "\n";
    }
}

#endif // BASIC_CONSUMER_B_H 