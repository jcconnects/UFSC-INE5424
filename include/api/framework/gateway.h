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
        typedef Network::Communicator Communicator;
        typedef Network::Protocol Protocol;
        typedef Protocol::Address Address;
        typedef Network::Message Message;
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
        void handle(Message* msg);
        void subscribe(Message* message);
        void publish(Message* message);

        unsigned int _id;
        Map _producers;
        Map _interests;
        Network* _network;
        Communicator* _comms;
        pthread_t _receive_thread;
        pthread_mutex_t _producers_mutex;
        pthread_mutex_t _interests_mutex;
        std::atomic<bool> _running;
};

/******** Gateway Implementation ********/
Gateway::Gateway(unsigned int id) : _id(id) {
    _network = new Network(id);
    
    // Sets communicator
    Address addr(_network->address(), id);
    _comms = new Communicator(_network->channel(), addr);

    pthread_mutex_init(&_producers_mutex, nullptr);
    pthread_mutex_init(&_interests_mutex, nullptr);

    _running = true;
    pthread_create(&_receive_thread, nullptr, Gateway::mainloop, this);
}

Gateway::~Gateway() {

    _running.store(false, std::memory_order_release);
    _comms->release();

    pthread_join(_receive_thread, nullptr);
    pthread_mutex_destroy(&_producers_mutex);
    pthread_mutex_destroy(&_interests_mutex);
    delete _comms;
    delete _network;
}

void Gateway::attach_interest(Observer* obs, Unit type) {
    pthread_mutex_lock(&_interests_mutex);
    _interests[type].insert(obs);
    pthread_mutex_unlock(&_interests_mutex);
}

void Gateway::detach_interest(Observer* obs, Unit type) {
    obs->update(nullptr); // Releases thread waiting for data

    pthread_mutex_lock(&_interests_mutex);
    auto it = _interests.find(type);

    if (it != _interests.end()) {
        it->second.erase(obs); // removes observer
        if (it->second.empty()) {
            _interests.erase(it);  // removes type if there are no observers
        }
    }
    pthread_mutex_unlock(&_interests_mutex);
}

void Gateway::attach_producer(Observer* obs, Unit type) {
    pthread_mutex_lock(&_producers_mutex);
    _producers[type].insert(obs);
    pthread_mutex_unlock(&_producers_mutex);
}

bool Gateway::send(Message* message) {
    if (message->size() > MAX_MESSAGE_SIZE) {
        return false;
    }

    bool result = _comms->send(message);
    handle(message);

    return result;
}

bool Gateway::receive(Message* message) {
    return _comms->receive(message);
}

void Gateway::subscribe(Message* buf) {
    Unit uid = buf->unit();

    pthread_mutex_lock(&_producers_mutex);
    auto it = _producers.find(uid);
    
    if (it != _producers.end()) {
        for (auto* obs : it->second) {
            Message* msg = new Message(*buf);
            obs->update(msg);
        }
    }
    pthread_mutex_lock(&_producers_mutex);
}

void Gateway::publish(Message* buf) {
    Unit uid = buf->unit();

    pthread_mutex_lock(&_interests_mutex);
    auto it = _interests.find(uid);

    if (it != _interests.end()) {
        for (auto* obs : it->second) {
            Message* msg = new Message(*buf);
            obs->update(msg);
        }
    }
    pthread_mutex_unlock(&_interests_mutex);
}

void Gateway::handle(Message* message) {
    switch (message->message_type())
    {
        case Message::Type::INTEREST:
            subscribe(message);
            break;
        case Message::Type::RESPONSE:
            publish(message);
            break;
        default:
            break;
    }
}

void* Gateway::mainloop(void* arg) {
    Gateway* self = reinterpret_cast<Gateway*>(arg);

    while (self->running()) {
        Message msg;
        if (self->receive(&msg)) {
            self->handle(&msg);
        }
    }
    return nullptr;
}

bool Gateway::running() const {
    return _running.load(std::memory_order_acquire);
}

const Gateway::Address& Gateway::address() {
    return _comms->address();
}

#endif // GATEWAY_H