#ifndef GATEWAY_H
#define GATEWAY_H

#include <unordered_map>
#include <unordered_set>
#include <stdexcept>
#include <pthread.h>

#include "api/network/communicator.h"
#include "api/framework/network.h"
#include "api/util/observer.h"
#include "api/network/message.h"

class Gateway {
    public:
        typedef Network::Protocol Protocol;
        typedef Communicator<Protocol> Comms;
        typedef Message<Protocol> Message;
        typedef Protocol::Address Address;
        typedef Message::Unit Unit;
        typedef Concurrent_Observer<Message, void> Observer;
        typedef std::unordered_map<Unit, std::unordered_set<Observer*>> Map;

        static const unsigned int MAX_MESSAGE_SIZE = Protocol::MTU;

        Gateway(unsigned int id);
        ~Gateway();

        void attach_interest(Observer* obs, Unit type);
        void detach_interest(Observer* obs, Unit type);
        void attach_producer(Observer* obs, Unit type);

        bool send(Message* message);
        bool receive(Message* msg);
        
        bool running() const;
        static void* mainloop(void* arg);
        
        const Address& address();

    private:
        static constexpr std::uint32_t EXTERNAL_BIT = 0x80000000; // Bit 31
        static constexpr std::uint32_t UNIT_MASK    = 0x7FFFFFFF; // Bits 0–30
        
        static inline bool is_external(Unit unit);
        
        void handle(Message* msg);
        bool subscribe(Message* message);
        bool publish(Message* message);

        unsigned int _id;
        Map _producers;
        Map _interests;
        Network* _network;
        Comms* _comms;
        pthread_t _receive_thread;
        std::atomic<bool> _running;
};

/******** Gateway Implementation ********/
Gateway::Gateway(unsigned int id) : _id(id) {
    _network = new Network(id);
    
    // Sets communicator
    Address addr(_network->address(), id);
    _comms = new Comms(_network->channel(), addr);

    _running = true;
    pthread_create(&_receive_thread, nullptr, Gateway::mainloop, this);
}

Gateway::~Gateway() {

    _running.store(false, std::memory_order_release);
    _comms->release();

    pthread_join(_receive_thread, nullptr);
    delete _comms;
    delete _network;
}

void Gateway::attach_interest(Observer* obs, Unit type) {
    _interests[type].insert(obs);
}

void Gateway::detach_interest(Observer* obs, Unit type) {
    obs->update(nullptr); // Releases thread waiting for data

    auto it = _interests.find(type);

    if (it != _interests.end()) {
        it->second.erase(obs); // removes observer
        if (it->second.empty()) {
            _interests.erase(it);  // removes type if there are no observers
        }
    }
}

void Gateway::attach_producer(Observer* obs, Unit type) {
    _producers[type].insert(obs);
}

bool Gateway::send(Message* message) {
    if (message->size() > MAX_MESSAGE_SIZE) {
        return false;
    }

    if (is_external(message->unit())) {
        return _comms->send(message);
    }
    
    handle(message);
}

bool Gateway::receive(Message* message) {
    return _comms->receive(message);
}

bool Gateway::subscribe(Message* message) {
    Unit uid = message->unit();
    auto it = _producers.find(uid);

    if (it != _producers.end()) {
        for (auto* obs : it->second) {
            obs->update(message);
        }
        return true;
    }
    return false;
}

void Gateway::handle(Message* msg) {
    switch (msg->message_type())
    {
        case Message::Type::INTEREST:
            subscribe(msg);
            break;
        case Message::Type::RESPONSE:
            publish(msg);
            break;
        default:
            break;
    }
}

bool Gateway::publish(Message* message) {
    Unit uid = message->unit();
    auto it = _interests.find(uid);

    if (it != _interests.end()) {
        for (auto* obs : it->second) {
            obs->update(message);
        }
        return true;
    }
    return false;
}

void* Gateway::mainloop(void* arg) {
    Gateway* self = reinterpret_cast<Gateway*>(arg);

    while (self->running()) {
        Message msg;
        if (self->receive(&msg)) {
            self->handle(&msg);
        }
    }
}

bool Gateway::running() const {
    return _running.load(std::memory_order_acquire);
}

const Gateway::Address& Gateway::address() {
    return _comms->address();
}

bool Gateway::is_external(Unit unit) {
    return unit & EXTERNAL_BIT;
}

#endif // GATEWAY_H