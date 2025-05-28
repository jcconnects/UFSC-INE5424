#ifndef GATEWAY_H
#define GATEWAY_H

#include <unordered_map>
#include <unordered_set>
#include <stdexcept>
#include <pthread.h>
#include <chrono>
#include <sstream>

#include "../network/communicator.h"
#include "../network/bus.h"
#include "network.h"
#include "../util/observer.h"
#include "../util/csv_logger.h"
#include "../network/message.h"
#include "../util/debug.h"
#include <signal.h>


void handler(int sig) {
    // No action needed, just unblock from system calls
}

class Gateway {
    public:
        typedef Network::Communicator Communicator;
        typedef Network::Protocol Protocol;
        typedef Protocol::Address Address;
        typedef Network::Message Message;
        typedef Message::Unit Unit;
        typedef CAN::Observer Observer;
        typedef std::unordered_map<Unit, std::unordered_set<Observer*>> Map;

        static const unsigned int MAX_MESSAGE_SIZE = Protocol::MTU;
        const unsigned int PORT = 0;

        Gateway(const unsigned int id);
        ~Gateway();

        bool send(Message* message);
        bool receive(Message* msg);
        bool internalReceive(Message* msg);
        
        bool running() const;
        static void* mainloop(void* arg);
        static void* internalLoop(void* arg);

        void set_handler();
        
        const Address& address();
        CAN* bus() { return _can; }
        
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
Gateway::Gateway(const unsigned int id) : _id(id) {
    db<Gateway>(TRC) << "Gateway::Gateway(" << id << ") called!\n";
    
    _network = new Network(id);
    
    // Sets communicator
    Address addr(_network->address(), PORT);
    _comms = new Communicator(_network->channel(), addr);
    _can = _network->bus();
    Condition c(0, Message::Type::UNKNOWN);
    _can_observer = new Observer(c);
    
    // CRITICAL FIX: Attach observer to CAN bus
    _can->attach(_can_observer, c);
    
    db<Gateway>(INF) << "[Gateway " << _id << "] created with address: " << addr.to_string() << "\n";

    _running = true;
    pthread_create(&_receive_thread, nullptr, Gateway::mainloop, this);
    pthread_create(&_internal_thread, nullptr, Gateway::internalLoop, this);
    
    db<Gateway>(INF) << "[Gateway " << _id << "] threads started\n";
}

Gateway::~Gateway() {
    db<Gateway>(TRC) << "Gateway::~Gateway() called for ID " << _id << "!\n";
    _running.store(false, std::memory_order_release);
    
    // CRITICAL FIX: Detach observer from CAN bus before deleting
    if (_can_observer) {
        Condition c(0, Message::Type::UNKNOWN);
        _can->detach(_can_observer, c);
        delete _can_observer;
        _can_observer = nullptr;
    }
    delete _network;
    
    _comms->release();

    // Send signals to interrupt threads
    pthread_kill(_receive_thread, SIGUSR1);
    pthread_kill(_internal_thread, SIGUSR1);
    
    // Actually join the threads before deleting resources
    pthread_join(_receive_thread, nullptr);
    pthread_join(_internal_thread, nullptr);

    delete _comms;
    
    db<Gateway>(INF) << "[Gateway " << _id << "] destroyed successfully\n";
}

bool Gateway::send(Message* message) {
    if (message->size() > MAX_MESSAGE_SIZE) {
        db<Gateway>(WRN) << "[Gateway " << _id << "] message too large: " << message->size() << " > " << MAX_MESSAGE_SIZE << "\n";
        return false;
    }

    db<Gateway>(INF) << "[Gateway " << _id << "] sending external message of type " << static_cast<int>(message->message_type()) 
                     << " for unit " << message->unit() << "\n";

    // Log sent message to CSV
    log_message(*message, "SEND");

    bool result = _comms->send(message);
    
    db<Gateway>(INF) << "[Gateway " << _id << "] external send result: " << (result ? "SUCCESS" : "FAILED") << "\n";
    
    return result;
}

bool Gateway::receive(Message* message) {
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
void Gateway::handle(Message* message) {
    // CRITICAL FIX: Check if message originated from this gateway to prevent feedback loop
    if (message->origin() == _comms->address()) {
        db<Gateway>(INF) << "[Gateway " << _id << "] ignoring message from self (origin: " 
                         << message->origin().to_string() << ", self: " << _comms->address().to_string() << ")\n";
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
        default:
            db<Gateway>(WRN) << "[Gateway " << _id << "] unknown message type: " << static_cast<int>(modified_message.message_type()) << "\n";
            break;
    }
}

void* Gateway::mainloop(void* arg) {
    Gateway* self = reinterpret_cast<Gateway*>(arg);
    self->set_handler();
    
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

bool Gateway::internalReceive(Message* msg) {
    // CRITICAL FIX: Get message from observer and copy it to the parameter
    Message* received_msg = _can_observer->updated();
    if (!received_msg) {
        return false;
    }
    
    // Copy the received message to the output parameter
    *msg = *received_msg;
    
    db<Gateway>(INF) << "[Gateway " << _id << "] received internal message of type " << static_cast<int>(msg->message_type()) 
                     << " for unit " << msg->unit() << "\n";
    
    // Clean up the received message
    delete received_msg;
    
    return true;
}

void* Gateway::internalLoop(void* arg) {
    Gateway* self = reinterpret_cast<Gateway*>(arg);
    self->set_handler();
    
    db<Gateway>(INF) << "[Gateway " << self->_id << "] internal receive loop started\n";

    while (self->running()) {
        Message msg;
        if (self->internalReceive(&msg)) {
            // CRITICAL FIX: Check if message originated from this gateway to prevent feedback loop
            if (msg.origin() == self->_comms->address()) {
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

void Gateway::set_handler() {
    // Install the signal handler for thread interruption
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, nullptr);

    if (sigaction(SIGUSR1, &sa, nullptr) == -1) {
        throw std::runtime_error("Failed to set signal handler for SIGUSR1");
    }
}

bool Gateway::running() const {
    return _running.load(std::memory_order_acquire);
}

const Gateway::Address& Gateway::address() {
    return _comms->address();
}

void Gateway::setup_csv_logging(const std::string& log_dir) {
    std::string csv_file = log_dir + "/gateway_" + std::to_string(_id) + "_messages.csv";
    std::string header = "timestamp_us,message_type,direction,origin,destination,unit,period_us,value_size,latency_us";
    _csv_logger = std::make_unique<CSVLogger>(csv_file, header);
}

void Gateway::log_message(const Message& msg, const std::string& direction) {
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
             << (direction == "SEND" ? address().to_string() : msg.origin().to_string()) << ","
             << (direction == "SEND" ? "NETWORK" : address().to_string()) << ","
             << msg.unit() << ","
             << msg.period().count() << ","
             << msg.value_size() << ","
             << latency_us;
    
    _csv_logger->log(csv_line.str());
}

#endif // GATEWAY_H