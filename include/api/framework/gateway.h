#ifndef GATEWAY_H
#define GATEWAY_H

#include <unordered_map>
#include <unordered_set>
#include <pthread.h>
#include <chrono>
#include <sstream>

#include "api/traits.h"
#include "api/network/communicator.h"
#include "api/network/bus.h"
#include "api/framework/network.h"
#include "api/util/csv_logger.h"
#include "api/network/message.h"
#include "api/util/debug.h"

class Gateway {
    public:
        typedef Network::Communicator Communicator;
        typedef Network::Protocol Protocol;
        typedef Protocol::Address Address;
        typedef Network::Message Message;
        typedef Message::Unit Unit;
        typedef CAN::Observer Observer;
        typedef std::unordered_map<Unit, std::unordered_set<Observer*>> Map;

        inline static const unsigned int MAX_MESSAGE_SIZE = Protocol::MTU - sizeof(Protocol::Header) - sizeof(Protocol::TimestampFields);
        const unsigned int PORT = 0;

        Gateway(const unsigned int id, Network::EntityType entity_type = Network::EntityType::VEHICLE);
        ~Gateway();

        void start();

        bool send(Message* message);
        bool receive(Message* msg);
        bool internalReceive(Message* msg);
        
        bool running() const;
        static void* mainloop(void* arg);
        static void* internalLoop(void* arg);
        
        const Address& address();
        CAN* bus() { return _can; }
        
        // New method to access network for RSU manager setup
        Network* network() { return _network; }
        
        // CSV logging methods
        void setup_csv_logging(const std::string& log_dir);
        void log_message(const Message& msg, const std::string& direction);

    private:
        void handle(Message* msg);
        void subscribe(Message* message);
        void publish(Message* message);

        unsigned int _id;
        Network* _network;
        Communicator* _comms;
        CAN* _can;
        Observer* _can_observer;
        pthread_t _receive_thread;
        pthread_t _internal_thread;
        std::atomic<bool> _running;
        std::unique_ptr<CSVLogger> _csv_logger;
};

/******** Gateway Implementation ********/
inline Gateway::Gateway(const unsigned int id, Network::EntityType entity_type) : _id(id), _running(false) {
    db<Gateway>(TRC) << "Gateway::Gateway(" << id << ", entity_type) called!\n";
    _network = new Network(id, entity_type);
    
    // Sets communicator
    Address addr(_network->address(), PORT);
    _comms = new Communicator(_network->channel(), addr);
    _can = _network->bus();
    Condition c(0, Message::Type::UNKNOWN);
    _can_observer = new Observer(c);
    
    // CRITICAL FIX: Attach observer to CAN bus
    _can->attach(_can_observer, c);
    
    db<Gateway>(INF) << "[Gateway " << _id << "] created with address: " << addr.to_string() << "\n";
}

inline void Gateway::start() {
    db<Gateway>(TRC) << "Gateway::start() called for ID " << _id << "!\n";
    if (_running.load()) {
        db<Gateway>(WRN) << "[Gateway " << _id << "] start() called but already running.\n";
        return;
    }
    _running.store(true, std::memory_order_release);
    pthread_create(&_receive_thread, nullptr, Gateway::mainloop, this);
    pthread_create(&_internal_thread, nullptr, Gateway::internalLoop, this);
    db<Gateway>(INF) << "[Gateway " << _id << "] threads started\n";
}

inline Gateway::~Gateway() {
    db<Gateway>(TRC) << "Gateway::~Gateway() called for ID " << _id << "!\n";
    if (!_running.load()) {
        return;
    }
    _running.store(false, std::memory_order_release);

    _comms->release();

    // CRITICAL FIX: Detach observer from CAN bus before deleting
    if (_can_observer) {
        Message* dummy_msg = new Message();
        Condition c(0, Message::Type::UNKNOWN);
        _can_observer->update(c, dummy_msg);
        _can->detach(_can_observer, c);
    }
    pthread_join(_internal_thread, nullptr);
    delete _can_observer;
    _can_observer = nullptr;

    _network->stop();
    pthread_join(_receive_thread, nullptr);
    delete _network;
    db<Gateway>(TRC) << "[Gateway " << _id << "] threads joined\n";

    delete _comms;
    
    db<Gateway>(INF) << "[Gateway " << _id << "] destroyed successfully\n";
}

inline bool Gateway::send(Message* message) {

    if (message->size() > MAX_MESSAGE_SIZE) {
        db<Gateway>(WRN) << "[Gateway " << _id << "] message too large: " << message->size() << " > " << MAX_MESSAGE_SIZE << "\n";
        return false;
    }

    db<Gateway>(INF) << "[Gateway " << _id << "] sending external message of type " << static_cast<int>(message->message_type()) 
                     << " for unit " << message->unit() << "\n";

    // Log sent message to CSV
    log_message(*message, "SEND");

    if (!running()) {
        db<Gateway>(WRN) << "[Gateway " << _id << "] send called but gateway is not running\n";
        return false;
    }
    bool result = _comms->send(message);
    
    db<Gateway>(INF) << "[Gateway " << _id << "] external send result: " << (result ? "SUCCESS" : "FAILED") << "\n";
    
    return result;
}

inline bool Gateway::receive(Message* message) {
    if (!_running.load(std::memory_order_acquire)) {
        db<Gateway>(WRN) << "[Gateway " << _id << "] receive called but gateway is not running\n";
        return false;
    }

    bool result = _comms->receive(message);
    
    // Log received message to CSV if successful
    if (result) {
        db<Gateway>(INF) << "[Gateway " << _id << "] received external message of type " << static_cast<int>(message->message_type()) 
                         << " for unit " << message->unit() << "\n";
        log_message(*message, "RECEIVE");
    }
    
    return result;
}

// TODO - Edit origin in message
inline void Gateway::handle(Message* message) {
    // CRITICAL FIX: Check if message originated from this gateway to prevent feedback loop
    if (message->origin() == _comms->address()) {
        db<Gateway>(INF) << "[Gateway " << _id << "] ignoring message from self (origin: " 
                         << message->origin().to_string() << ", self: " << _comms->address().to_string() << ")\n";
        return;
    }
    
    // Validate message type to detect corruption
    auto msg_type = message->message_type();
    if (msg_type != Message::Type::INTEREST && 
        msg_type != Message::Type::RESPONSE && 
        msg_type != Message::Type::STATUS &&
        msg_type != Message::Type::REQ &&
        msg_type != Message::Type::KEY_RESPONSE) {
        db<Gateway>(ERR) << "[Gateway " << _id << "] received corrupted message with invalid type " 
                         << static_cast<int>(msg_type) << " from origin " << message->origin().to_string() 
                         << ", unit=" << message->unit() << ", period=" << message->period().count() 
                         << ", value_size=" << message->value_size() << " - DROPPING MESSAGE\n";
        return;
    }
    
    db<Gateway>(INF) << "[Gateway " << _id << "] handling external message of type " << static_cast<int>(message->message_type()) 
                     << " for unit " << message->unit() << " from origin " << message->origin().to_string() << "\n";
    

    Message modified_message = *message; 
    modified_message.origin(_comms->address());

    switch (modified_message.message_type())
    {
        case Message::Type::INTEREST:
            db<Gateway>(INF) << "[Gateway " << _id << "] forwarding INTEREST to CAN bus with modified origin\n";
            _can->send(&modified_message);
            break;
        case Message::Type::RESPONSE:
            db<Gateway>(INF) << "[Gateway " << _id << "] forwarding RESPONSE to CAN bus with modified origin\n";
            _can->send(&modified_message);
            break;
        case Message::Type::STATUS:
            db<Gateway>(INF) << "[Gateway " << _id << "] forwarding STATUS to CAN bus with modified origin\n";
            _can->send(&modified_message);
            break;
        default:
            db<Gateway>(WRN) << "[Gateway " << _id << "] unknown message type: " << static_cast<int>(modified_message.message_type()) << "\n";
            break;
    }
}

inline void* Gateway::mainloop(void* arg) {
    Gateway* self = reinterpret_cast<Gateway*>(arg);
    
    db<Gateway>(INF) << "[Gateway " << self->_id << "] external receive loop started\n";

    while (self->running()) {
        Message msg;
        if (self->receive(&msg)) {
            self->handle(&msg);
        }
    }
    
    db<Gateway>(INF) << "[Gateway " << self->_id << "] external receive loop ended\n";
    return nullptr;
}

inline bool Gateway::internalReceive(Message* msg) {
    // CRITICAL FIX: Get message from observer and copy it to the parameter
    Message* received_msg = _can_observer->updated();
    if (!received_msg) {
        db<Gateway>(WRN) << "[Gateway " << _id << "] no internal message received\n";
        return false;
    }
    
    // Copy the received message to the output parameter
    *msg = *received_msg;
    
    db<Gateway>(INF) << "[Gateway " << _id << "] received internal message of type " << static_cast<int>(msg->message_type()) 
                     << " for unit " << msg->unit() << " external: " << msg->external() << "\n";
    
    // Clean up the received message
    delete received_msg;
    
    return true;
}

inline void* Gateway::internalLoop(void* arg) {
    Gateway* self = reinterpret_cast<Gateway*>(arg);
    
    db<Gateway>(INF) << "[Gateway " << self->_id << "] internal receive loop started\n";

    while (self->running()) {
        Message msg;
        if (self->internalReceive(&msg)) {
            // CRITICAL FIX: Check if message originated from this gateway to prevent feedback loop
            if (msg.origin() == self->_comms->address() || !msg.external()) {
                db<Gateway>(INF) << "[Gateway " << self->_id << "] ignoring internal message from self (origin: " 
                                 << msg.origin().to_string() << ", self: " << self->_comms->address().to_string() << ")\n";
                continue;
            }
            
            db<Gateway>(INF) << "[Gateway " << self->_id << "] forwarding internal message externally from origin " 
                             << msg.origin().to_string() << "\n";
            self->send(&msg);
        }
    }
    
    db<Gateway>(INF) << "[Gateway " << self->_id << "] internal receive loop ended\n";
    return nullptr;
}

inline bool Gateway::running() const {
    return _running.load(std::memory_order_acquire);
}

inline const Gateway::Address& Gateway::address() {
    return _comms->address();
}

inline void Gateway::setup_csv_logging(const std::string& log_dir) {
    std::string csv_file = log_dir + "/gateway_" + std::to_string(_id) + "_messages.csv";
    std::string header = "timestamp_us,message_type,direction,origin,destination,unit,period_us,value_size,latency_us";
    _csv_logger = std::make_unique<CSVLogger>(csv_file, header);
}

inline void Gateway::log_message(const Message& msg, const std::string& direction) {
    if (!_csv_logger || !_csv_logger->is_open()) return;
    
    auto timestamp_us = Message::getSynchronizedTimestamp().count();
    
    // Calculate latency for received messages
    auto latency_us = 0L;
    if (direction == "RECEIVE") {
        latency_us = timestamp_us - msg.timestamp().count();
    }
    
    // Properly map message types instead of assuming unknown types are STATUS
    std::string msg_type_str;
    switch (msg.message_type()) {
        case Message::Type::INTEREST:
            msg_type_str = "INTEREST";
            break;
        case Message::Type::RESPONSE:
            msg_type_str = "RESPONSE";
            break;
        case Message::Type::STATUS:
            msg_type_str = "STATUS";
            break;
        case Message::Type::REQ:
            msg_type_str = "REQ";
            break;
        case Message::Type::KEY_RESPONSE:
            msg_type_str = "KEY_RESPONSE";
            break;
        case Message::Type::UNKNOWN:
            msg_type_str = "UNKNOWN";
            break;
        case Message::Type::INVALID:
            msg_type_str = "INVALID";
            break;
        default:
            msg_type_str = "CORRUPTED_TYPE_" + std::to_string(static_cast<int>(msg.message_type()));
            break;
    }
    
    std::ostringstream csv_line;
    csv_line << timestamp_us << ","
             << msg_type_str << ","
             << direction << ","
             << (direction == "SEND" ? address().to_string() : msg.origin().to_string()) << ","
             << (direction == "SEND" ? "NETWORK" : address().to_string()) << ","
             << msg.unit() << ","
             << msg.period().count() << ","
             << msg.value_size() << ","
             << latency_us;
    
    _csv_logger->log(csv_line.str());
}

#endif // GATEWAY_H