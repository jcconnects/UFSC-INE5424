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
    
    // Make sure the vehicle is stopped before destruction
    if (_running) {
        stop();
    }
    
    // Components are already stopped in Vehicle::stop()
    // Now we can safely delete them
    for (auto component : _components) {
        delete component;
    }
    
    delete _comms;
    delete _protocol;
    delete _nic;

    db<Vehicle>(INF) << "[Vehicle " << std::to_string(_id) << "] destroyed.\n";
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
    
    // First mark as not running - new receive calls will fail quickly
    _running = false;
    
    // Log the stop intention to help with debugging
    db<Vehicle>(INF) << "[Vehicle " << std::to_string(_id) << "] stop initiated, will close connections and stop components\n";
    
    // Next, close the connections to unblock any blocked receive calls in component threads
    db<Vehicle>(TRC) << "[Vehicle " << std::to_string(_id) << "] closing connections to unblock receive calls\n";
    
    try {
        _comms->close();
        db<Vehicle>(TRC) << "[Vehicle " << std::to_string(_id) << "] connections closed successfully\n";
    } catch (const std::exception& e) {
        db<Vehicle>(ERR) << "[Vehicle " << std::to_string(_id) << "] error closing connections: " << e.what() << "\n";
    } catch (...) {
        db<Vehicle>(ERR) << "[Vehicle " << std::to_string(_id) << "] unknown error closing connections\n";
    }
    
    // Allow a longer delay for receive calls to return with errors
    db<Vehicle>(TRC) << "[Vehicle " << std::to_string(_id) << "] waiting for components to detect stop signal\n";
    usleep(200000); // 200 milliseconds
    
    // Now it's safe to stop components - threads won't be blocked on receive anymore
    db<Vehicle>(TRC) << "[Vehicle " << std::to_string(_id) << "] stopping all components\n";
    stop_components();
    
    db<Vehicle>(INF) << "[Vehicle " << std::to_string(_id) << "] all components stopped\n";
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

    // Don't attempt to send if the vehicle is stopping
    if (!_running) {
        db<Vehicle>(TRC) << "[Vehicle " << std::to_string(_id) << "] send() called after vehicle stopped\n";
        return 0;
    }

    Message<MAX_MESSAGE_SIZE> msg = Message<MAX_MESSAGE_SIZE>(data, size);
    
    // Check again before sending
    if (!_running) {
        return 0;
    }
    
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

    // Create a message for receiving data
    Message<MAX_MESSAGE_SIZE> msg = Message<MAX_MESSAGE_SIZE>();
    
    // Check running state again right before receive call
    if (!_running) {
        return 0;
    }
    
    // Attempt to receive a message
    bool receive_result = _comms->receive(&msg);
    
    // Check if we've been stopped while receiving
    if (!_running) {
        db<Vehicle>(TRC) << "[Vehicle " << std::to_string(_id) << "] vehicle stopped during receive\n";
        return 0;
    }

    // If receive failed, return appropriately
    if (!receive_result) {
        // Only log if vehicle is still running and hasn't been stopped
        if (_running) {
            db<Vehicle>(INF) << "[Vehicle " << std::to_string(_id) << "] message not received\n";
        }
        return 0;
    }

    // Copy received data to provided buffer
    if (msg.size() > size) {
        db<Vehicle>(ERR) << "[Vehicle " << std::to_string(_id) << "] Received message size exceeds buffer size " << std::to_string(size) << "\n";
        return 0;
    }

    std::memcpy(data, msg.data(), msg.size());
    
    // Only log success if we're still running
    if (_running) {
        db<Vehicle>(INF) << "[Vehicle " << std::to_string(_id) << "] message received\n";
    }

    return msg.size();
}


#endif // VEHICLE_H