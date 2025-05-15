#ifndef AGENT_H
#define AGENT_H


#include <cstdint>
#include <chrono>
#include <vector>

#include "initializer.h"
#include "communicator.h"
#include "message.h"

// Foward Declaration
class Periodic_Thread;

class Agent {
    public:
        enum class Mode {
            INTERNAL,
            EXTERNAL
        };

        typedef Initializer::Protocol_T Protocol;
        typedef Initializer::NIC_T NIC;
        typedef Communicator<Protocol> Comms;
        typedef Protocol::Address Address;

        Agent(Protocol* protocol, Address addr);
        ~Agent();

        virtual std::vector<std::uint8_t> get(std::uint32_t type) = 0;

        int send(Message::Type, std::uint32_t unit_type, std::chrono::microseconds period = std::chrono::microseconds::zero(), const void* value_data = nullptr, const unsigned int value_size = 0, Mode mode = Mode::EXTERNAL);
        int receive(const void* value_data, const unsigned int value_size);

        const std::uint32_t type();

    private:
        Comms* _comm;
        std::uint32_t _type;
        Periodic_Thread _thread;
        std::atomic<bool> _running;
};

/****** Agent Implementation *****/
Agent::Agent(Protocol* protocol, Address addr) {
    _comm = new Comms(protocol, addr);
}

Agent::~Agent() {
    _comm->close();
    delete _comm;
}

int Agent::receive(const void* value_data, const unsigned int value_size) {
    Message msg;

    if (_comm->receive(&msg)) {
        switch (msg.message_type())
        {
        case Message::Type::INTEREST:
            /* code */
            break;
        
        default:
            break;
        }
    }
}

#endif // AGENT_H