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
        
        void add_component(std::unique_ptr<Component> component);
        void start_components();
        void stop_components();

        // Get protocol for component to create its communicator
        Protocol<NIC<SocketEngine>>* protocol() const { return _protocol; }
        
        // Get next component address
        typename Protocol<NIC<SocketEngine>>::Address next_component_address();
    
    private:
        unsigned int _id;
        Protocol<NIC<SocketEngine>>* _protocol;
        NIC<SocketEngine>* _nic;
        
        // Base address for this vehicle
        typename Protocol<NIC<SocketEngine>>::Address _base_address;
        
        // Counter for assigning component addresses
        unsigned int _next_component_id;

        std::atomic<bool> _running;
        std::vector<std::unique_ptr<Component>> _components;
};

/******** Vehicle Implementation *********/
Vehicle::Vehicle(unsigned int id, NIC<SocketEngine>* nic, Protocol<NIC<SocketEngine>>* protocol) {
    db<Vehicle>(TRC) << "Vehicle::Vehicle() called!\n";

    _id = id;
    _nic = nic;
    _protocol = protocol;
    _next_component_id = 1; // Start component IDs from 1
    
    // Initialize base address with vehicle's NIC address
    _base_address = Protocol<NIC<SocketEngine>>::Address(nic->address(), 0);
}

// Include Component definition here, after Vehicle is defined
// but before we use component methods
#include "component.h"

Vehicle::~Vehicle() {
    db<Vehicle>(TRC) << "Vehicle::~Vehicle() called!\n";
    
    stop_components();

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
    std::cout << "[Vehicle " << _id << "] starting." << "\n";
    _running = true;
    start_components();
}

void Vehicle::stop() {
    db<Vehicle>(TRC) << "Vehicle::stop() called!\n";
    
    stop_components();

    if (_nic) {
        _nic->stop();
    }
    
    _running = false;
}

void Vehicle::add_component(std::unique_ptr<Component> component) {
    if(component) {
        _components.push_back(std::move(component));
    }
}

void Vehicle::start_components() {
    db<Vehicle>(TRC) << "Vehicle::start_components() called!\n";
    for (const auto& component_ptr : _components) {
        if (component_ptr) {
            component_ptr->start();
        }
    }
}

void Vehicle::stop_components() {
    db<Vehicle>(TRC) << "Vehicle::stop_components() called!\n";
    for (auto it = _components.rbegin(); it != _components.rend(); ++it) {
        if (*it) {
            (*it)->stop();
        }
    }
}

typename Protocol<NIC<SocketEngine>>::Address Vehicle::next_component_address() {
    // Create an address with the vehicle's physical address and next component ID as port
    typename Protocol<NIC<SocketEngine>>::Address addr = _base_address;
    addr.port(_next_component_id++);
    return addr;
}

#endif // VEHICLE_H