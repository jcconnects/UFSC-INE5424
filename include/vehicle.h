#ifndef VEHICLE_H
#define VEHICLE_H

#include <string>
#include <atomic>
#include <vector>
#include <memory>
#include <chrono>
#include <unistd.h>

#include "communicator.h"
#include "message.h"
#include "debug.h"

// Forward declarations
template <typename NIC>
class Protocol;

template <typename Engine>
class NIC;

class SocketEngine;
// Forward declaration of Component
class Component;

// Vehicle class definition
class Vehicle {

    public:
        static constexpr const unsigned int MAX_MESSAGE_SIZE = Communicator<Protocol<NIC<SocketEngine>>>::MAX_MESSAGE_SIZE;

        Vehicle(unsigned int id, NIC<SocketEngine>* nic, Protocol<NIC<SocketEngine>>* protocol);

        ~Vehicle();

        const unsigned int id() const;
        const bool running() const;

        void start();
        void stop();
        
        void add_component(Component* component);
        void start_components();
        void stop_components();

        int send(const void* data, unsigned int size);
        int receive(void* data, unsigned int size); 
    
    private:

        unsigned int _id;
        Protocol<NIC<SocketEngine>>* _protocol;
        NIC<SocketEngine>* _nic;
        Communicator<Protocol<NIC<SocketEngine>>>* _comms;

        std::atomic<bool> _running;
        std::vector<Component*> _components;
};

/******** Vehicle Implementation *********/
Vehicle::Vehicle(unsigned int id, NIC<SocketEngine>* nic, Protocol<NIC<SocketEngine>>* protocol) {
    db<Vehicle>(TRC) << "Vehicle::Vehicle() called!\n";

    _id = id;
    _nic = nic;
    _protocol = protocol;
    _comms = new Communicator<Protocol<NIC<SocketEngine>>>(protocol, Protocol<NIC<SocketEngine>>::Address(nic->address(), Protocol<NIC<SocketEngine>>::Address::NULL_VALUE));
}

// Include Component definition here, after Vehicle is defined
// but before we use component methods
#include "component.h"

Vehicle::~Vehicle() {
    db<Vehicle>(TRC) << "Vehicle::~Vehicle() called!\n";
    
    stop_components();

    for (auto component : _components) {
        delete component;
    }
    
    delete _comms;
    delete _protocol;
    delete _nic;
}

const unsigned int Vehicle::id() const {
    return _id;
}

const bool Vehicle::running() const {
    return _running;
}

void Vehicle::start() {
    db<Vehicle>(TRC) << "Vehicle::start() called!\n";

    _running = true;
    start_components();
}

void Vehicle::stop() {
    db<Vehicle>(TRC) << "Vehicle::stop() called!\n";
    
    // Stopping NIC
    _nic->stop();
    
    // Close connections to unblock receive calls
    db<Vehicle>(TRC) << "[Vehicle " << std::to_string(_id) << "] closing connections to unblock receive calls\n";
    _comms->close();

    stop_components();

    _running = false;
}

void Vehicle::add_component(Component* component) {
    _components.push_back(component);
}

void Vehicle::start_components() {
    db<Vehicle>(TRC) << "Vehicle::start_components() called!\n";
    for (auto component : _components) {
        component->start();
    }
}

void Vehicle::stop_components() {
    db<Vehicle>(TRC) << "Vehicle::stop_components() called!\n";
    for (auto component : _components) {
        component->stop();
    }
}

int Vehicle::send(const void* data, unsigned int size) {
    db<Vehicle>(TRC) << "Vehicle::send() called!\n";

    Message<MAX_MESSAGE_SIZE> msg = Message<MAX_MESSAGE_SIZE>(data, size);
    
    if (!_comms->send(&msg)) {
        db<Vehicle>(INF) << "[Vehicle " << std::to_string(_id) << "] message not sent\n";
        return 0;
    }
    
    db<Vehicle>(INF) << "[Vehicle " << std::to_string(_id) << "] message sent\n";
    return 1;
}

int Vehicle::receive(void* data, unsigned int size) {
    db<Vehicle>(TRC) << "Vehicle::receive() called!\n";

    if (!data || size == 0) {
        db<Vehicle>(ERR) << "Error: Invalid data buffer in receive\n";
        return 0;
    }
    
    // Check if we're still running before attempting to receive
    if (!_running) {
        db<Vehicle>(TRC) << "[Vehicle " << std::to_string(_id) << "] receive() called after vehicle stopped\n";
        return 0;
    }

    Message<MAX_MESSAGE_SIZE> msg = Message<MAX_MESSAGE_SIZE>();
    if (!_comms->receive(&msg)) {
        db<Vehicle>(INF) << "[Vehicle " << std::to_string(_id) << "] message not received\n";
        return 0;
    }

    // Copia os dados recebidos para o buffer fornecido
    if (msg.size() > size) {
        db<Vehicle>(ERR) << "[Vehicle " << std::to_string(_id) << "] Received message size exceeds buffer size " << std::to_string(size) << "\n";
        return 0;
    }

    std::memcpy(data, msg.data(), msg.size());
    db<Vehicle>(INF) << "[Vehicle " << std::to_string(_id) << "] message received\n";

    return msg.size();
}


#endif // VEHICLE_H