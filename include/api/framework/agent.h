#ifndef AGENT_H
#define AGENT_H

#include <cstdint>
#include <chrono>
#include <vector>
#include <algorithm>
#include <string>

#include "api/network/bus.h"
#include "api/framework/periodicThread.h"
#include "api/util/debug.h"
#include "api/traits.h"


class Agent {
    public:
        typedef CAN::Message Message;
        typedef Message::Origin Address;
        typedef Message::Array Value;
        typedef Message::Unit Unit;
        typedef Message::Type Type;
        typedef Message::Microseconds Microseconds;
        typedef Periodic_Thread<Agent> Thread;
        typedef CAN::Observer Observer;

    
        Agent(CAN* bus, const std::string& name, Address address = {});
        virtual ~Agent();

        // Vehicle adds on creation
        void add_observed_type(Unit unit, Type type) ;

        virtual Value get(Unit type) = 0;

        int send(Unit unit, Microseconds period); // Send will always be INTEREST (this is exposed to application)
        int receive(Message* msg);
        
        virtual void handle_response(Message* msg) { /* Default implementation - do nothing */}
        
        static void* run(void* arg); // Run will always receive INTEREST messages, and set periodic thread
        bool running();

        std::string name() const { return _name; }
        
    private:
        void handle_interest(Unit unit, Microseconds period);
        void reply(Unit unit);
    
    private:
        Address _address;
        CAN* _can;
        std::string _name;
        Observer* _can_observer;
        pthread_t _thread;
        Thread* _periodic_thread;
        std::atomic<bool> _running;
};

/****** Agent Implementation *****/
Agent::Agent(CAN* bus, const std::string& name, Address address) : _address(address), _name(name), _periodic_thread(nullptr) {
    db<Agent>(INF) << "[Agent] " << _name << " created with address: " << _address.to_string() << "\n";
    if (!bus)
        throw std::invalid_argument("Gateway cannot be null");
    
    _can = bus;

    _running = true;
    pthread_create(&_thread, nullptr, Agent::run, this);
}

Agent::~Agent() {
    _running.store(false, std::memory_order_release);
    pthread_join(_thread, nullptr);

    delete _can;
    delete _can_observer;
}

//Vehicle adds on creation
void Agent::add_observed_type(Unit unit, Type type) {
    db<Agent>(INF) << "[Agent] " << _name << " adding produced type: " << unit << " of type: " << static_cast<int>(type) << "\n";
    Condition c(unit, type);
    _can_observer = new Observer(c);
    _can->attach(_can_observer, c);
}

int Agent::send(Unit unit, Microseconds period) {
    db<Agent>(INF) << "[Agent] " << _name << " sending INTEREST for unit: " << unit << " with period: " << period.count() << " microseconds\n";
    if (period == Microseconds::zero())
        return 0;
    
    Message msg(Message::Type::INTEREST, _address, unit, period);
    int result = _can->send(&msg);

    if (!result)
        return -1; 

    // Gateway attaches automatically on receive
    // _gateway->attach_interest(&_response_obs, unit);

    return result;
}

int Agent::receive(Message* msg) {
    (*msg) = (*_can_observer->updated());

    return msg->value_size();
}


void* Agent::run(void* arg) {
    Agent* agent = reinterpret_cast<Agent*>(arg);

    while (agent->running()) {
        Message* msg = new Message();
        int received = agent->receive(msg);

        if (received <= 0) {
            db<Agent>(WRN) << "[Agent] " << agent->_name << " received an empty or invalid message\n";
            delete msg; // Clean up the message object

            continue; // Skip processing if no valid message was received
        }

        if (msg->message_type() == Message::Type::RESPONSE) 
            agent->handle_response(msg);
        else if (msg->message_type() == Message::Type::INTEREST) 
            agent->handle_interest(msg->unit(), msg->period());

        delete msg; // Clean up the message object after processing
    }

    return nullptr;
}

bool Agent::running() {
    return _running.load(std::memory_order_acquire);
}

void Agent::handle_interest(Unit unit, Microseconds period) {

    if (!_periodic_thread) {
        _periodic_thread = new Thread(this, &Agent::reply, unit);
    } else {
        // Ajusta o período com MDC entre o atual e o novo
        _periodic_thread->adjust_period(period.count());
    }
}


void Agent::reply(Unit unit) {
    db<Agent>(INF) << "[Agent] " << _name << " sending RESPONSE for unit: " << unit << "\n";
    Value value = get(unit);
    Message msg(Message::Type::RESPONSE, _address, unit, Microseconds::zero(), value.data(), value.size());

    _can->send(&msg);
}


#endif // AGENT_H