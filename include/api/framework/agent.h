#ifndef AGENT_H
#define AGENT_H

#include <cstdint>
#include <chrono>
#include <vector>
#include <algorithm>
#include <string>
#include <sstream>

#include "../network/bus.h"
#include "periodicThread.h"
#include "../util/debug.h"
#include "../util/csv_logger.h"
#include "../traits.h"


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

    
        Agent(CAN* bus, const std::string& name, Unit unit, Type type, Address address = {});
        virtual ~Agent();

        virtual Value get(Unit type) = 0;

        int send(Unit unit, Microseconds period); // Send will always be INTEREST (this is exposed to application)
        int receive(Message* msg);
        
        virtual void handle_response(Message* msg) { /* Default implementation - do nothing */}
        
        static void* run(void* arg); // Run will always receive INTEREST messages, and set periodic thread
        bool running();

        std::string name() const { return _name; }
        
        // CSV logging methods
        void set_csv_logger(const std::string& log_dir);
        void log_message(const Message& msg, const std::string& direction);
        
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
        Condition _c;
        std::unique_ptr<CSVLogger> _csv_logger;
};

/****** Agent Implementation *****/
Agent::Agent(CAN* bus, const std::string& name, Unit unit, Type type, Address address) : _address(address), _name(name), _periodic_thread(nullptr) {
    db<Agent>(INF) << "[Agent] " << _name << " created with address: " << _address.to_string() << "\n";
    if (!bus)
        throw std::invalid_argument("Gateway cannot be null");
    
    _can = bus;
    Condition c(unit, type);
    _c = c;
    _can_observer = new Observer(c);
    _can->attach(_can_observer, c);

    // Only send initial INTEREST if this agent is a consumer (observing RESPONSE messages)
    // Producers (observing INTEREST messages) should not send INTEREST
    if (type == Type::RESPONSE) {
        Microseconds period(1000000);
        send(unit, period); // Send initial INTEREST
    }

    _running = true;
    int result = pthread_create(&_thread, nullptr, Agent::run, this);
    if (result != 0) {
        _running = false;
        throw std::runtime_error("Failed to create agent thread");
    }
}

Agent::~Agent() {
    // First, stop the running flag to prevent new operations
    _running.store(false, std::memory_order_release);
    
    // Stop periodic thread first if it exists - CRITICAL: Do this before anything else
    if (_periodic_thread) {
        _periodic_thread->join();
        delete _periodic_thread;
        _periodic_thread = nullptr;
    }
    
    // Send a dummy message to wake up the main thread if it's waiting
    Message* dummy_msg = new Message();
    _can_observer->update(_c, dummy_msg);
    
    // Wait for the main thread to finish
    pthread_join(_thread, nullptr);
    
    // Detach from CAN bus before deleting observer
    if (_can_observer) {
        _can->detach(_can_observer, _c);
        delete _can_observer;
        _can_observer = nullptr;
    }
    
    // Clean up the dummy message
    delete dummy_msg;
    
    db<Agent>(INF) << "[Agent] " << _name << " destroyed successfully\n";
}

int Agent::send(Unit unit, Microseconds period) {
    db<Agent>(INF) << "[Agent] " << _name << " sending INTEREST for unit: " << unit << " with period: " << period.count() << " microseconds\n";
    if (period == Microseconds::zero())
        return 0;
    
    Message msg(Message::Type::INTEREST, _address, unit, period);
    
    // Log sent message to CSV
    log_message(msg, "SEND");
    
    int result = _can->send(&msg);

    if (!result)
        return -1; 

    return result;
}

int Agent::receive(Message* msg) {
    db<Agent>(INF) << "[Agent] " << _name << " waiting for messages...\n";
    (*msg) = *(_can_observer->updated());

    // Log received message to CSV
    log_message(*msg, "RECEIVE");

    return msg->size();
}


void* Agent::run(void* arg) {
    Agent* agent = reinterpret_cast<Agent*>(arg);

    while (agent->running()) {
        Message* msg = new Message();
        int received = agent->receive(msg);

        if (received <= 0 || !msg) {
            db<Agent>(WRN) << "[Agent] " << agent->name() << " received an empty (received=" << received << ") or invalid message\n";
            delete msg; // Clean up the message object

            continue; // Skip processing if no valid message was received
        }

        db<Agent>(INF) << "[Agent] " << agent->name() << " received message of type: " << static_cast<int>(msg->message_type()) 
                       << " for unit: " << msg->unit() << " with size: " << msg->value_size() << "\n";

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
    db<Agent>(INF) << "[Agent] " << _name << " received INTEREST for unit: " << unit << " with period: " << period.count() << " microseconds\n";
    
    // Only respond to INTEREST if this agent is a producer (observing INTEREST messages)
    // Consumers (observing RESPONSE messages) should not respond to INTEREST
    if (_c.type() != Type::INTEREST) {
        db<Agent>(WRN) << "[Agent] " << _name << " ignoring INTEREST message (not a producer)\n";
        return;
    }
    
    if (!_periodic_thread) {
        _periodic_thread = new Thread(this, &Agent::reply, unit);
        _periodic_thread->start(period.count()); // Actually start the thread!
    } else {
        // Ajusta o período com MDC entre o atual e o novo
        _periodic_thread->adjust_period(period.count());
    }
}


void Agent::reply(Unit unit) {
    // Safety check: don't reply if agent is being destroyed
    if (!running()) {
        return;
    }
    
    // Additional safety check: ensure periodic thread is still valid
    if (!_periodic_thread || !_periodic_thread->running()) {
        return;
    }
    
    db<Agent>(INF) << "[Agent] " << _name << " sending RESPONSE for unit: " << unit << "\n";
    
    // Final safety check before calling virtual method
    if (!running()) {
        return;
    }
    
    Value value = get(unit);
    Message msg(Message::Type::RESPONSE, _address, unit, Microseconds::zero(), value.data(), value.size());

    // Log sent message to CSV
    log_message(msg, "SEND");

    _can->send(&msg);
}

void Agent::set_csv_logger(const std::string& log_dir) {
    std::string csv_file = log_dir + "/" + _name + "_messages.csv";
    std::string header = "timestamp_us,message_type,direction,origin,destination,unit,period_us,value_size,latency_us";
    _csv_logger = std::make_unique<CSVLogger>(csv_file, header);
}

void Agent::log_message(const Message& msg, const std::string& direction) {
    if (!_csv_logger || !_csv_logger->is_open()) return;
    
    auto now = std::chrono::system_clock::now();
    auto timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
    
    // Calculate latency for received messages
    auto latency_us = 0L;
    if (direction == "RECEIVE") {
        latency_us = timestamp_us - msg.timestamp().count();
    }
    
    std::ostringstream csv_line;
    csv_line << timestamp_us << ","
             << (msg.message_type() == Message::Type::INTEREST ? "INTEREST" : "RESPONSE") << ","
             << direction << ","
             << (direction == "SEND" ? _address.to_string() : msg.origin().to_string()) << ","
             << (direction == "SEND" ? "BROADCAST" : _address.to_string()) << ","
             << msg.unit() << ","
             << msg.period().count() << ","
             << msg.value_size() << ","
             << latency_us;
    
    _csv_logger->log(csv_line.str());
}

#endif // AGENT_H