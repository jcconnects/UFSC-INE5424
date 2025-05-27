#ifndef GATEWAY_H
#define GATEWAY_H

#include <unordered_map>
#include <unordered_set>
#include <stdexcept>
#include <pthread.h>

#include "api/network/communicator.h"
#include "api/network/bus.h"
#include "api/framework/network.h"
#include "api/util/observer.h"
#include "api/network/message.h"
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
};

/******** Gateway Implementation ********/
Gateway::Gateway(const unsigned int id) : _id(id) {
    _network = new Network(id);
    
    // Sets communicator
    Address addr(_network->address(), PORT);
    _comms = new Communicator(_network->channel(), addr);
    _can = _network->bus();
    Condition c(0, Message::Type::UNKNOWN);
    _can_observer = new Observer(c);

    _running = true;
    pthread_create(&_receive_thread, nullptr, Gateway::mainloop, this);
    pthread_create(&_internal_thread, nullptr, Gateway::internalLoop, this);
}

Gateway::~Gateway() {

    _running.store(false, std::memory_order_release);
    _comms->release();

    pthread_kill(_receive_thread, SIGUSR1);
    pthread_kill(_internal_thread, SIGUSR1);

    delete _comms;
    delete _can_observer;
    delete _network;
}

bool Gateway::send(Message* message) {
    if (message->size() > MAX_MESSAGE_SIZE) {
        return false;
    }

    bool result = _comms->send(message);
    // maybe we should not call handle here
    // handle(message);

    return result;
}

bool Gateway::receive(Message* message) {
    return _comms->receive(message);
}

// TODO - Edit origin in message
void Gateway::handle(Message* message) {
    switch (message->message_type())
    {
        case Message::Type::INTEREST:
            _can->send(message);
            break;
        case Message::Type::RESPONSE:
            _can->send(message);
            break;
        default:
            break;
    }
}

void* Gateway::mainloop(void* arg) {
    Gateway* self = reinterpret_cast<Gateway*>(arg);
    self->set_handler();

    while (self->running()) {
        Message msg;
        if (self->receive(&msg)) {
            self->handle(&msg);
        }
    }
    return nullptr;
}

bool Gateway::internalReceive(Message* msg) {
    msg = _can_observer->updated();
    if (!msg) 
        return false;
    
    return false;
}

void* Gateway::internalLoop(void* arg) {
    Gateway* self = reinterpret_cast<Gateway*>(arg);
    self->set_handler();

    while (self->running()) {
        Message msg;
        if (self->internalReceive(&msg)) {
            self->send(&msg);
        }
    }
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

#endif // GATEWAY_H