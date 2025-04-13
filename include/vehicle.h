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
    _running = false;
    _comms = new Communicator<Protocol<NIC<SocketEngine>>>(_protocol, Protocol<NIC<SocketEngine>>::Address(_nic->address(), Protocol<NIC<SocketEngine>>::Address::NULL_VALUE));
}

// Include Component definition here, after Vehicle is defined
// but before we use component methods
#include "component.h"

Vehicle::~Vehicle() {
    db<Vehicle>(TRC) << "Vehicle::~Vehicle() called!\n";
    
    for (auto component : _components) {
        db<Vehicle>(TRC) << "[Vehicle " << _id << "] Deleting component " << component->name() << "\n";
        delete component;
    }
    _components.clear();
    
    db<Vehicle>(TRC) << "[Vehicle " << _id << "] Deleting communicator\n";
    delete _comms;
    db<Vehicle>(TRC) << "[Vehicle " << _id << "] Deleting protocol\n";
    delete _protocol;
    
    // Engine thread should already be stopped in Vehicle::stop()
    // so we can safely delete NIC now
    
    db<Vehicle>(TRC) << "[Vehicle " << _id << "] Deleting NIC\n";
    delete _nic;
    db<Vehicle>(INF) << "[Vehicle " << _id << "] Vehicle resources deleted.\n";
}

const unsigned int Vehicle::id() const {
    return _id;
}

const bool Vehicle::running() const {
    return _running;
}

void Vehicle::start() {
    db<Vehicle>(TRC) << "Vehicle::start() called!\n";

    if (!_running) {
        _running = true;
        start_components();
    }
}

void Vehicle::stop() {
    db<Vehicle>(TRC) << "Vehicle::stop() called!\n";
    
    if (!_running) {
        db<Vehicle>(TRC) << "[Vehicle " << _id << "] Vehicle already stopped or stopping.\n";
        return;
    }

    db<Vehicle>(INF) << "[Vehicle " << _id << "] Signaling components to stop.\n";
    _running = false;

    // *** Stop the Engine Thread FIRST ***
    // Ensure the background network processing stops before anything else
    db<Vehicle>(INF) << "[Vehicle " << _id << "] Stopping NIC engine thread...\n";
    if (_nic) {
        _nic->stop(); // This calls SocketEngine::stop which should now block until the engine thread is joined
        db<Vehicle>(INF) << "[Vehicle " << _id << "] NIC engine thread stopped.\n";
    }

    // Close communicator connections to unblock any threads in receive()
    db<Vehicle>(INF) << "[Vehicle " << _id << "] Closing communicator connections.\n";
    if (_comms) {
        _comms->close();
        
        // Add a small delay to ensure the close signal propagates
        db<Vehicle>(TRC) << "[Vehicle " << _id << "] Waiting briefly for close signal to propagate...\n";
        usleep(10000); // 10ms delay
    } else {
        db<Vehicle>(WRN) << "[Vehicle " << _id << "] Communicator was null during stop.\n";
    }

    db<Vehicle>(INF) << "[Vehicle " << _id << "] Stopping components...\n";
    stop_components();
    db<Vehicle>(INF) << "[Vehicle " << _id << "] All components stopped and joined.\n";

    db<Vehicle>(INF) << "[Vehicle " << _id << "] Vehicle stop sequence complete.\n";
}

void Vehicle::add_component(Component* component) {
    _components.push_back(component);
}

void Vehicle::start_components() {
    db<Vehicle>(TRC) << "Vehicle::start_components() called!\n";
    for (auto component : _components) {
        db<Vehicle>(TRC) << "[Vehicle " << _id << "] Starting component " << component->name() << "\n";
        component->start();
    }
}

void Vehicle::stop_components() {
    db<Vehicle>(TRC) << "Vehicle::stop_components() called!\n";
    for (auto component : _components) {
        db<Vehicle>(TRC) << "[Vehicle " << _id << "] Stopping component " << component->name() << "\n";
        component->stop();
    }
    db<Vehicle>(TRC) << "[Vehicle " << _id << "] Finished calling stop on all components.\n";
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
        return -1;
    }
    
    if (!_running) {
        db<Vehicle>(TRC) << "[Vehicle " << std::to_string(_id) << "] receive() called after vehicle stopped\n";
        return 0;
    }

    Message<MAX_MESSAGE_SIZE> msg = Message<MAX_MESSAGE_SIZE>();
    if (!_comms->receive(&msg)) {
        if (!_running) {
            db<Vehicle>(TRC) << "[Vehicle " << _id << "] message not received (vehicle stopped during receive)\n";
        } else {
            db<Vehicle>(INF) << "[Vehicle " << _id << "] message not received (possible error or timeout)\n";
        }
        return 0;
    }

    if (msg.size() > size) {
        db<Vehicle>(ERR) << "[Vehicle " << std::to_string(_id) << "] Received message size exceeds buffer size " << std::to_string(size) << "\n";
        return -2;
    }

    std::memcpy(data, msg.data(), msg.size());
    db<Vehicle>(INF) << "[Vehicle " << std::to_string(_id) << "] message received\n";

    return msg.size();
}


#endif // VEHICLE_H