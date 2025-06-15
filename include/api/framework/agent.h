#ifndef AGENT_HPP
#define AGENT_HPP

#include <string>
#include <sstream>
#include <memory>
#include <atomic>
#include <cassert>
#include <stdexcept>
#include <pthread.h>

#include "../network/bus.h"
#include "periodicThread.h"
#include "../util/debug.h"
#include "../util/csv_logger.h"
#include "../traits.h"
#include "component_types.hpp"
#include "component_functions.hpp"

/**
 * @brief EPOS-inspired Agent implementation using composition over inheritance
 * 
 * This Agent class eliminates the "pure virtual method called" race condition
 * by using function pointers instead of virtual methods. Following EPOS SmartData
 * principles, components are pure data + function pairs rather than inheritance-based.
 * 
 * Key improvements:
 * - No virtual methods = no vtable race conditions
 * - Function-based composition instead of inheritance
 * - Type-safe component data management
 * - Same public API as original Agent for compatibility
 */
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

    
        Agent(CAN* bus, const std::string& name, Unit unit, Type type, Address address,
              DataProducer producer, ResponseHandler handler, 
              std::unique_ptr<ComponentData> data);
        ~Agent();

        // Non-virtual interface - eliminates race condition
        Value get(Unit unit);
        void handle_response(Message* msg);

        int send(Unit unit, Microseconds period); // Send will always be INTEREST (this is exposed to application)
        int receive(Message* msg);
        
        static void* run(void* arg); // Run will always receive INTEREST messages, and set periodic thread
        bool running();

        std::string name() const { return _name; }
        
        // CSV logging methods
        void set_csv_logger(const std::string& log_dir);
        void log_message(const Message& msg, const std::string& direction);
        
        int start_periodic_interest(Unit unit, Microseconds period);
        void stop_periodic_interest();
        void send_interest(Unit unit);
        void update_interest_period(Microseconds new_period);
        
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
        
        Thread* _interest_thread;              // Periodic thread for sending INTEREST
        Microseconds _requested_period;        // Period requested by this consumer
        std::atomic<bool> _interest_active;    // Control interest sending
        std::atomic<bool> _is_consumer;        // Track if this agent is a consumer
        
        // EPOS-inspired composition members
        std::unique_ptr<ComponentData> _component_data;
        DataProducer _data_producer;
        ResponseHandler _response_handler;
};

/****** Agent Implementation *****/

/**
 * @brief Constructor for EPOS-inspired Agent using composition
 * 
 * Creates an Agent that uses function pointers instead of virtual methods,
 * eliminating the vtable race condition during destruction.
 * 
 * @param bus CAN bus for communication
 * @param name Agent name for identification
 * @param unit Data unit this agent handles
 * @param type Agent type (INTEREST for producers, RESPONSE for consumers)
 * @param address Network address
 * @param producer Function pointer for data production (can be nullptr for consumers)
 * @param handler Function pointer for response handling (can be nullptr for producers)
 * @param data Component-specific data structure
 */
inline Agent::Agent(CAN* bus, const std::string& name, Unit unit, Type type, Address address,
                    DataProducer producer, ResponseHandler handler, 
                    std::unique_ptr<ComponentData> data) 
    : _address(address), _name(name), _periodic_thread(nullptr), _interest_thread(nullptr), 
      _requested_period(Microseconds::zero()), _interest_active(false), 
      _is_consumer(type == Type::RESPONSE), _component_data(std::move(data)),
      _data_producer(producer), _response_handler(handler) {
    
    db<Agent>(INF) << "[Agent] " << _name << " created with address: " << _address.to_string() << "\n";
    if (!bus)
        throw std::invalid_argument("Gateway cannot be null");
    
    // Validate agent role and function pointer consistency
    if (_is_consumer) {
        if (!handler) {
            throw std::invalid_argument("Consumer agents must have a response handler");
        }
        if (producer) {
            throw std::invalid_argument("Consumer agents should not have a data producer");
        }
    } else {
        if (!producer) {
            throw std::invalid_argument("Producer agents must have a data producer");
        }
        if (handler) {
            throw std::invalid_argument("Producer agents should not have a response handler");
        }
    }
    
    _can = bus;
    Condition c(unit, type);
    _c = c;
    _can_observer = new Observer(c);
    _can->attach(_can_observer, c);

    // Phase 1.2: Consumer initialization - No automatic INTEREST sending
    if (_is_consumer) {
        // Don't send initial INTEREST here anymore
        // Application will call start_periodic_interest() when ready
        db<Agent>(INF) << "[Agent] " << _name << " initialized as consumer, waiting for application to start periodic interest\n";
    } else {
        db<Agent>(INF) << "[Agent] " << _name << " initialized as producer, ready to handle INTEREST messages\n";
    }

    _running = true;
    int result = pthread_create(&_thread, nullptr, Agent::run, this);
    if (result != 0) {
        _running = false;
        throw std::runtime_error("Failed to create agent thread");
    }
}

/**
 * @brief Destructor with proper cleanup order
 * 
 * Ensures threads are properly joined before object destruction,
 * eliminating the race condition that occurred with virtual methods.
 */
inline Agent::~Agent() {
    // First, stop the running flag to prevent new operations
    _running.store(false, std::memory_order_release);
    
    if (_interest_thread) {
        stop_periodic_interest();
    }
    
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

/**
 * @brief Get data using function pointer instead of virtual method
 * 
 * This is the key method that eliminates the race condition. Instead of
 * calling a virtual method that could crash during destruction, we call
 * a function pointer that was set during construction.
 * 
 * @param unit The data unit being requested
 * @return Value containing the generated data
 */
inline Agent::Value Agent::get(Unit unit) {
    // Only producers should generate data
    if (_is_consumer || !_data_producer || !_component_data) {
        return Value(); // Return empty vector if not a producer
    }
    return _data_producer(unit, _component_data.get());
}

/**
 * @brief Handle response using function pointer instead of virtual method
 * 
 * Similar to get(), this uses a function pointer to avoid virtual method
 * calls that could cause race conditions during destruction.
 * 
 * @param msg Pointer to the received message
 */
inline void Agent::handle_response(Message* msg) {
    // Only consumers should handle responses
    if (!_is_consumer || !_response_handler || !_component_data) {
        return; // Silently ignore if not a consumer
    }
    _response_handler(msg, _component_data.get());
}

inline int Agent::send(Unit unit, Microseconds period) {
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

inline int Agent::receive(Message* msg) {
    db<Agent>(INF) << "[Agent] " << _name << " waiting for messages...\n";
    (*msg) = *(_can_observer->updated());

    // Log received message to CSV
    log_message(*msg, "RECEIVE");

    return msg->size();
}

inline void* Agent::run(void* arg) {
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

inline bool Agent::running() {
    return _running.load(std::memory_order_acquire);
}

inline void Agent::handle_interest(Unit unit, Microseconds period) {
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

/**
 * @brief Reply method that calls function pointer instead of virtual method
 * 
 * This is the critical method where the race condition used to occur.
 * Instead of calling the virtual get() method, we now call the function
 * pointer directly, eliminating the vtable dependency.
 */
inline void Agent::reply(Unit unit) {
    // Safety check: don't reply if agent is being destroyed
    if (!running()) {
        return;
    }
    
    // Additional safety check: ensure periodic thread is still valid
    if (!_periodic_thread || !_periodic_thread->running()) {
        return;
    }
    
    db<Agent>(INF) << "[Agent] " << _name << " sending RESPONSE for unit: " << unit << "\n";
    
    // Final safety check before calling function pointer
    if (!running()) {
        return;
    }
    
    // CRITICAL: Call function pointer instead of virtual method
    // This eliminates the race condition that caused "pure virtual method called"
    Value value = get(unit);
    Message msg(Message::Type::RESPONSE, _address, unit, Microseconds::zero(), value.data(), value.size());

    // Log sent message to CSV
    log_message(msg, "SEND");

    _can->send(&msg);
}

inline void Agent::set_csv_logger(const std::string& log_dir) {
    std::string csv_file = log_dir + "/" + _name + "_messages.csv";
    std::string header = "timestamp_us,message_type,direction,origin,destination,unit,period_us,value_size,latency_us";
    _csv_logger = std::make_unique<CSVLogger>(csv_file, header);
}

inline void Agent::log_message(const Message& msg, const std::string& direction) {
    if (!_csv_logger || !_csv_logger->is_open()) return;
    
    auto timestamp_us = Message::getSynchronizedTimestamp().count();
    
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

/**
 * @brief Start sending periodic INTEREST messages for the specified unit and period
 * 
 * @param unit The data type unit to request
 * @param period The desired response period from producers
 * @return int Success/failure status
 */
inline int Agent::start_periodic_interest(Unit unit, Microseconds period) {
    if (!_is_consumer) {
        db<Agent>(WRN) << "[Agent] " << _name << " is not a consumer, cannot start periodic interest\n";
        return -1;
    }
    
    if (_interest_active.load()) {
        db<Agent>(INF) << "[Agent] " << _name << " updating interest period from " 
                       << _requested_period.count() << " to " << period.count() << " microseconds\n";
        update_interest_period(period);
        return 0;
    }
    
    _requested_period = period;
    _interest_active.store(true, std::memory_order_release);
    
    if (!_interest_thread) {
        _interest_thread = new Thread(this, &Agent::send_interest, unit);
        _interest_thread->start(period.count());
        db<Agent>(INF) << "[Agent] " << _name << " started periodic INTEREST for unit: " 
                       << unit << " with period: " << period.count() << " microseconds\n";
    }
    
    return 0;
}

/**
 * @brief Stop sending periodic INTEREST messages
 */
inline void Agent::stop_periodic_interest() {
    if (_interest_active.load()) {
        _interest_active.store(false, std::memory_order_release);
        
        if (_interest_thread) {
            _interest_thread->join();
            delete _interest_thread;
            _interest_thread = nullptr;
        }
        
        db<Agent>(INF) << "[Agent] " << _name << " stopped periodic INTEREST\n";
    }
}

/**
 * @brief Send a single INTEREST message (called by periodic thread)
 * 
 * @param unit The data type unit
 */
inline void Agent::send_interest(Unit unit) {
    if (!_interest_active.load() || !running()) {
        return;
    }
    
    db<Agent>(TRC) << "[Agent] " << _name << " sending periodic INTEREST for unit: " 
                   << unit << " with period: " << _requested_period.count() << " microseconds\n";
    
    Message msg(Message::Type::INTEREST, _address, unit, _requested_period);
    log_message(msg, "SEND");
    _can->send(&msg);
}

/**
 * @brief Update the period for periodic interest sending
 * 
 * @param new_period The new period to use
 */
inline void Agent::update_interest_period(Microseconds new_period) {
    _requested_period = new_period;
    if (_interest_thread) {
        _interest_thread->adjust_period(new_period.count());
    }
}

#endif // AGENT_HPP