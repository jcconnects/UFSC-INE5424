#ifndef AGENT_H
#define AGENT_H

#include <cstdint>
#include <chrono>
#include <vector>
#include <algorithm>

#include "initializer.h"
#include "communicator.h"
#include "message.h"
#include "periodicThread.h"
#include "observer.h"


class Agent : Concurrent_Observer<Message, Message::Unit> {
    public:
        typedef Initializer::Protocol_T Protocol;
        typedef Initializer::NIC_T NIC;
        typedef Communicator<Protocol> Comms;
        typedef Protocol::Address Address;
        typedef Periodic_Thread<Agent> Thread;
        typedef Message::ByteArray Value;
        typedef Message::Unit Unit;
        typedef Message::Microseconds Microseconds;
        typedef Concurrent_Observer<Message, Message::Unit> Observer;
        typedef Concurrent_Observer<Message, Message::Type> ResponseObserver;
    
        Agent(Protocol* protocol, Address addr, Unit type);
        ~Agent();

        virtual Value get(Unit type) = 0;

        void reply();
        int send(Unit unit_type, Microseconds period, const bool is_internal); // Send will always be INTEREST (this is exposed to application)
        int receive(void* value_data, unsigned int value_size);

        void handle_interest(Microseconds period);
        void handle_response(Message* msg);

        const Unit type();

        static void* run(void* arg);
        bool running();

    private:
        Comms* _comms;
        Unit _type;
        Thread* _periodic_thread;
        ResponseObserver _obs;
        pthread_t _thread;
        std::atomic<bool> _running;
        std::vector<Unit> _interests;
};

/****** Agent Implementation *****/
Agent::Agent(Protocol* protocol, Address addr, Unit type) : _type(type), Observer(type), _obs(Message::Type::RESPONSE) {
    _periodic_thread = new Thread(this, &Agent::reply);
    _comms = new Comms(protocol, addr);
    _comms->attach(this, _type);

    _running = true;
    pthread_create(&_thread, nullptr, Agent::run, this);
}

Agent::~Agent() {
    _comms->detach(this, _type);

    _running.store(false, std::memory_order_release);
    pthread_join(_thread, nullptr);

    _periodic_thread->join();

    delete _periodic_thread;
    delete _comms;
}

void* Agent::run(void* arg) {
    Agent* agent = reinterpret_cast<Agent*>(arg);

    while (agent->running()) {
        Message* msg = agent->updated();
        
        switch (msg->message_type()) {
            case Message::Type::INTEREST:
                agent->handle_interest(msg->period());
                break;
            case Message::Type::RESPONSE:
                agent->handle_response(msg);
                break;
            // case Message::Type::PTP:
            // case Message::Type::J:
            default:
                break;
        }
    }
}

void Agent::handle_interest(Microseconds period) {
    if (_periodic_thread->running()) {
        _periodic_thread->adjust_period(period);
    } else {
        _periodic_thread->start(period);
    }
}

void Agent::handle_response(Message* msg) {
    if (std::find(_interests.begin(), _interests.end(), msg->unit_type()) != _interests.end())
        _obs.update(msg->message_type(), msg);
}

bool Agent::running() {
    return _running.load(std::memory_order_acquire);
}

void Agent::reply() {
    // TODO: If the periodic thread replies EVERYONE, that means that all external broadcasts are also internal broadcasts, but none internal broadcast will be ever external
    Value value = get(_type);
    Message msg(Message::Type::RESPONSE, _comms->address(), _type, Microseconds::zero(), value.data(), value.size());
}

int Agent::send(Unit unit_type, Microseconds period, const bool is_internal) {
    if (period == Microseconds::zero())
        return 0;
    
    Message msg(Message::Type::INTEREST, _comms->address(), unit_type, period);
    int result = _comms->send(msg, is_internal);

    if (result)
        _interests.push_back(unit_type);
    
        return result;
}

int Agent::receive(void* value_data, unsigned int value_size) {
    bool received = false;

    Message* msg = _obs.updated();
    
    if (msg->value_size() > value_size) {
        return -1;
    }

    std::memcpy(value_data, msg->value(), msg->value_size());
    return 0;
}


#endif // AGENT_H