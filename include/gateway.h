#ifndef GATEWAY_H
#define GATEWAY_H

#include <unordered_map>
#include <unordered_set>
#include <stdexcept>
#include <pthread.h>

#include "communicator.h"
#include "initializer.h"
#include "observer.h"
#include "message.h"

// Foward declaration

class Gateway {
    public:
        typedef Initializer::Protocol_T Protocol;
        typedef Protocol::Physical_Address MAC_Address;
        typedef Protocol::Address Address;
        typedef Communicator<Protocol> Comms;
        typedef Message::Unit Unit;
        typedef Concurrent_Observer<Message, void> Observer;
        typedef std::unordered_map<Unit, std::unordered_set<Observer*>> Map;

        static const unsigned int MAX_MESSAGE_SIZE = Protocol::MTU;

        Gateway(Protocol* protocol, MAC_Address addr);
        ~Gateway();

        void register_interest(Observer* obs, Unit type);
        void register_producer(Observer* obs, Unit type);

        bool send(Message& message);
        bool receive(Message* msg);
        
        bool running() const;
        static void* mainloop(void* arg);
        
    private:
        static constexpr std::uint32_t EXTERNAL_BIT = 0x80000000; // Bit 31
        static constexpr std::uint32_t UNIT_MASK    = 0x7FFFFFFF; // Bits 0–30
        
        static inline bool is_external(Unit unit);
        
        bool subscribe(Message* message);
        bool publish(Message* message);
        void handle(Message* msg);

        Map _producers;
        Map _interests;
        Comms* _comms;
        pthread_t _receive_thread;
        std::atomic<bool> _running;
};

/******** Gateway Implementation ********/
Gateway::Gateway(Protocol* protocol, MAC_Address mac_addr) {
    if (!protocol)
        throw std::invalid_argument("Protocol cannot be null!");
    
    // Sets own address
    Address addr(mac_addr, 0);
    _comms = new Comms(protocol, addr);

    _running = true;
    pthread_create(&_receive_thread, nullptr, Gateway::mainloop, this);
}

Gateway::~Gateway() {

    _running.store(false, std::memory_order_release);
    _comms->release();

    pthread_join(_receive_thread, nullptr);
    delete _comms;
}

void Gateway::register_interest(Observer* obs, Unit type) {
    _interests[type].insert(obs);
}

void Gateway::register_producer(Observer* obs, Unit type) {
    _producers[type].insert(obs);
}

bool Gateway::send(Message& message) {
    if (message.size() > MAX_MESSAGE_SIZE) {
        return false;
    }

    if (is_external(message.unit_type())) {
        return _comms->send(message);
    }
    
    handle(&message);
}

bool Gateway::receive(Message* message) {
    return _comms->receive(message);
}

bool Gateway::subscribe(Message* message) {
    Unit uid = message->unit_type();
    auto it = _producers.find(uid);

    if (it != _producers.end()) {
        for (auto* obs : it->second) {
            obs->update(message);
        }
        return true;
    }
    return false;
}

bool Gateway::publish(Message* message) {
    Unit uid = message->unit_type();
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

void Gateway::handle(Message* msg) {
    switch (msg->message_type())
    {
        case Message::Type::PTP:
            // TODO: P4 implementation (prolly the call of PTP handler)
            break;
        case Message::Type::JOIN:
            // TODO: P5 implementation (prolly the call of Security handler)
            break;
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

bool Gateway::running() const {
    return _running.load(std::memory_order_acquire);
}

bool Gateway::is_external(Unit unit) {
    return unit & EXTERNAL_BIT;
}

#endif // GATEWAY_H