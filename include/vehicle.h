#ifndef VEHICLE_H
#define VEHICLE_H

#include <string>
#include <atomic>
#include <vector>
#include <memory>
#include <chrono>
#include <unistd.h>
#include <thread>
#include <stdexcept>

#include "communicator.h"
#include "message.h"
#include "debug.h"
#include "component.h"

// Forward declarations
// template <typename NIC> // No longer needed
// class Protocol;
// template <typename Engine> // No longer needed
// class NIC;
// class SocketEngine; // No longer needed directly

// Forward declaration of Component is already in component.h (included above)
// class Component;

// Assuming types are now defined in component.h or a types.h included by component.h
// We just need the forward declaration for Component itself.

// Vehicle class definition
class Vehicle {

    public:
        // Use the alias defined in component.h (or types.h)
        static constexpr const unsigned int MAX_MESSAGE_SIZE = TheCommunicator::MAX_MESSAGE_SIZE;

        // Update constructor signature to use the concrete types/aliases
        Vehicle(unsigned int id, TheNIC* nic, TheProtocol* protocol);

        ~Vehicle();

        const unsigned int id() const;
        const bool running() const;

        void start();
        void stop();

        void add_component(std::unique_ptr<Component> component);
        void start_components();
        void stop_components();

        // Update return type
        TheAddress address() const { return _base_address; }

        // Update return type
        TheProtocol* protocol() const { return _protocol; }

        // Update return type
        TheAddress next_component_address();

    private:
        unsigned int _id;
        // Update member types
        TheProtocol* _protocol;
        TheNIC* _nic;

        // Base address for this vehicle
        // Update member type
        TheAddress _base_address;

        // Counter for assigning component addresses
        unsigned int _next_component_id;

        std::atomic<bool> _running;
        std::vector<std::unique_ptr<Component>> _components;
};

/******** Vehicle Implementation *********/
// Update constructor signature and types used within
Vehicle::Vehicle(unsigned int id, TheNIC* nic, TheProtocol* protocol)
    : _id(id),
      _protocol(protocol),
      _nic(nic),
      _next_component_id(1), // Start component IDs from 1
      _running(false)
{
    db<Vehicle>(TRC) << "Vehicle::Vehicle() called!\n";

    if (!nic || !protocol) {
        throw std::invalid_argument("Vehicle requires non-null NIC and Protocol pointers.");
    }

    // Initialize base address with vehicle's NIC address and port 0
    _base_address = TheAddress(nic->address(), 0);
     db<Vehicle>(INF) << "[Vehicle " << _id << "] created with base address: " << _base_address << "\n";
}

Vehicle::~Vehicle() {
    db<Vehicle>(TRC) << "Vehicle::~Vehicle() called for ID " << _id << "!\n";

    // Ensure components and NIC/Protocol are stopped before deletion
    // Explicit stop() might be called externally, but ensure it happens.
    if (running()) { // Check if running before attempting stop
        stop();
    } else {
        // Even if not "running", ensure components are signaled stopped
        // and NIC/Protocol resources are released if they were partially started.
        stop_components(); // Signals components
        if (_nic) _nic->stop(); // Stop NIC if it exists
    }

    // Components are managed by unique_ptr, destruction is automatic.
    _components.clear(); // Explicitly clear vector

    // Protocol and NIC are owned by Vehicle in this design
    delete _protocol; // Protocol should be deleted before NIC? Check dependencies.
                     // Usually Protocol uses NIC, so delete Protocol first.
    delete _nic;
    _protocol = nullptr;
    _nic = nullptr;
    db<Vehicle>(INF) << "[Vehicle " << _id << "] Protocol and NIC deleted.\n";
}

const unsigned int Vehicle::id() const {
    return _id;
}

const bool Vehicle::running() const {
    return _running.load(std::memory_order_acquire);
}

void Vehicle::start() {
    db<Vehicle>(TRC) << "Vehicle::start() called for ID " << _id << "!\n";
    if (running()) {
        db<Vehicle>(WRN) << "[Vehicle " << _id << "] start() called but already running.\n";
        return;
    }
    std::cout << "[Vehicle " << _id << "] starting." << "\n";
    // NIC/Protocol/Engines are assumed to be started externally or by NIC constructor now
    // So Vehicle::start only needs to manage its own state and components.
    _running.store(true, std::memory_order_release);
    start_components();
    db<Vehicle>(INF) << "[Vehicle " << _id << "] started.\n";
}

void Vehicle::stop() {
    db<Vehicle>(TRC) << "Vehicle::stop() called for ID " << _id << "!\n";

    if (!running()) {
        db<Vehicle>(WRN) << "[Vehicle " << _id << "] stop() called but not running.\n";
        return;
    }

    _nic->stop();

    // Order: Stop components first, then the communication stack (NIC)
    // This follows the principle of stopping input/processing before the underlying comms.
    db<Vehicle>(INF) << "[Vehicle " << _id << "] Stopping components...\n";
    stop_components();

    db<Vehicle>(INF) << "[Vehicle " << _id << "] Stopping NIC...\n";
    if (_nic) {
        _nic->stop(); // NIC::stop() should handle stopping engines and its event loop
        db<Vehicle>(INF) << "[Vehicle " << _id << "] NIC stopped.\n";
    } else {
         db<Vehicle>(WRN) << "[Vehicle " << _id << "] NIC pointer was null during stop().\n";
    }

    _running.store(false, std::memory_order_release); // Mark vehicle as stopped
     db<Vehicle>(INF) << "[Vehicle " << _id << "] stopped.\n";
}

void Vehicle::add_component(std::unique_ptr<Component> component) {
    if(component) {
        db<Vehicle>(INF) << "[Vehicle " << _id << "] Adding component: " << component->getName() << "\n";
        _components.push_back(std::move(component));
    } else {
        db<Vehicle>(WRN) << "[Vehicle " << _id << "] Attempted to add null component.\n";
    }
}

void Vehicle::start_components() {
    db<Vehicle>(TRC) << "Vehicle::start_components() called for ID " << _id << "!\n";
    if (_components.empty()) {
         db<Vehicle>(INF) << "[Vehicle " << _id << "] No components to start.\n";
         return;
    }
    db<Vehicle>(INF) << "[Vehicle " << _id << "] Starting " << _components.size() << " components...\n";
    for (const auto& component_ptr : _components) {
        if (component_ptr) {
            db<Vehicle>(TRC) << "[Vehicle " << _id << "] Starting component: " << component_ptr->getName() << "\n";
            component_ptr->start(); // Component::start() creates the thread
        }
    }
    db<Vehicle>(INF) << "[Vehicle " << _id << "] All components requested to start.\n";
}

void Vehicle::stop_components() {
    db<Vehicle>(TRC) << "Vehicle::stop_components() called for ID " << _id << "!\n";
    if (_components.empty()) {
         db<Vehicle>(INF) << "[Vehicle " << _id << "] No components to stop.\n";
         return;
    }
    // Stop components in reverse order of addition/start
    db<Vehicle>(INF) << "[Vehicle " << _id << "] Stopping " << _components.size() << " components...\n";
    for (auto it = _components.rbegin(); it != _components.rend(); ++it) {
        if (*it) {
             db<Vehicle>(TRC) << "[Vehicle " << _id << "] Stopping component: " << (*it)->getName() << "\n";
            (*it)->stop(); // Component::stop() signals and joins the thread
        }
    }
    db<Vehicle>(INF) << "[Vehicle " << _id << "] All components stopped.\n";
}

// Update return type
TheAddress Vehicle::next_component_address() {
    // Create an address with the vehicle's physical address and next component ID as port
    TheAddress addr = _base_address; // Copy base address (NIC MAC + Port 0)
    addr.port(_next_component_id++); // Set the port to the next available ID
    db<Vehicle>(INF) << "[Vehicle " << _id << "] Generated next component address: " << addr.to_string() << "\n";
    return addr;
}

#endif // VEHICLE_H