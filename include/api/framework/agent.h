#ifndef AGENT_H
#define AGENT_H

#include <cstdint>
#include <chrono>
#include <vector>
#include <algorithm>

#include "api/framework/gateway.h"
#include "api/framework/periodicThread.h"
#include "api/util/observer.h"


class Agent {
    public:
        typedef Gateway::Message Message;
        typedef Message::Array Value;
        typedef Message::Unit Unit;
        typedef Message::Microseconds Microseconds;
        typedef Periodic_Thread<Agent> Thread;
        typedef std::unordered_map<Unit, Thread> Threads;
        typedef Concurrent_Observer<Message, void> Observer;
    
        Agent(Gateway* protocol);
        virtual ~Agent();

        void add_produced_type(Unit type);

        virtual Value get(Unit type) = 0;

        int send(Unit unit, Microseconds period); // Send will always be INTEREST (this is exposed to application)
        int receive(void* value_data, unsigned int value_size); // Receive will aways be RESPONSE (this is exposed to application)
        
        void handle_response(Message* msg);
        
        static void* run(void* arg); // Run will always receive INTEREST messages, and set periodic thread
        bool running();
        
    private:
        void handle_interest(Unit unit, Microseconds period);
        void reply(Unit unit);
    
    private:
        Gateway* _gateway;
        Threads _periodic_threads;
        Observer _interest_obs;
        Observer _response_obs;
        pthread_t _thread;
        Threads _threads;
        std::atomic<bool> _running;
};

/****** Agent Implementation *****/
Agent::Agent(Gateway* gateway) {
    if (!gateway)
        throw std::invalid_argument("Gateway cannot be null");
    
    _gateway = gateway;

    _running = true;
    pthread_create(&_thread, nullptr, Agent::run, this);
}

Agent::~Agent() {
    _running.store(false, std::memory_order_release);
    pthread_join(_thread, nullptr);

    delete _gateway;
}

void Agent::add_produced_type(Unit unit) {
    _gateway->attach_producer(&_interest_obs, unit);
}

int Agent::send(Unit unit, Microseconds period) {
    if (period == Microseconds::zero())
        return 0;
    
    Message msg(Message::Type::INTEREST, _gateway->address(), unit, period);
    int result = _gateway->send(&msg);

    if (!result)
        return -1; 

    _gateway->attach_interest(&_response_obs, unit);

    return result;
}

int Agent::receive(void* value_data, unsigned int value_size) {
    bool received = false;

    Message* msg = _response_obs.updated();
    
    if (msg->value_size() > value_size) {
        return -1;
    }

    std::memcpy(value_data, msg->value(), msg->value_size());
    return msg->value_size();
}


void* Agent::run(void* arg) {
    Agent* agent = reinterpret_cast<Agent*>(arg);

    while (agent->running()) {
        Message* msg = agent->_interest_obs.updated();
        agent->handle_interest(msg->unit(), msg->period());
    }
}

bool Agent::running() {
    return _running.load(std::memory_order_acquire);
}

void Agent::handle_interest(Unit unit, Microseconds period) {
    auto it = _periodic_threads.find(unit);

    if (it == _periodic_threads.end()) {
        Thread thread(this, &Agent::reply, unit);

        thread.start(period);

        _threads.emplace(unit, std::move(thread));
    } else {
        // Ajusta o período com MDC entre o atual e o novo
        it->second.adjust_period(period);
    }
}


void Agent::reply(Unit unit) {
    Value value = get(unit);
    Message msg(Message::Type::RESPONSE, _gateway->address(), unit, Microseconds::zero(), value.data(), value.size());
}


#endif // AGENT_H