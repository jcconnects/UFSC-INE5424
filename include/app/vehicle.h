#ifndef VEHICLE_H
#define VEHICLE_H

#include <atomic> // for std::atomic
#include <vector> // for std::vector
#include <memory> // for std::unique_ptr

#include "api/util/debug.h"
#include "api/framework/gateway.h"
#include "api/framework/agent.h"


// Vehicle class definition
class Vehicle {
    public:
        enum class Port : Gateway::Protocol::Port {
            BROADCAST = 0,
            CAMERA,
            ECU,
            LIDAR,
            INS,
        };

        // Update constructor signature to use the concrete types/aliases
        Vehicle(unsigned int id);
        ~Vehicle();

        const unsigned int id() const;
        const bool running() const;

        void start();
        void stop();

        template <typename ComponentType>
        void create_component(const std::string& name);
        
    private:
        unsigned int _id;
        Gateway* _gateway;
        std::atomic<bool> _running;
        std::vector<std::unique_ptr<Agent>> _components;
};

/******** Vehicle Implementation *********/
Vehicle::Vehicle(unsigned int id) : _id(id), _running(false)
{
    _gateway = new Gateway(_id);
}

Vehicle::~Vehicle() {
    // Ensure components and NIC/Protocol are stopped before deletion
    // Explicit stop() might be called externally, but ensure it happens.
    if (running()) { // Check if running before attempting stop
        stop();
    }

    // Components are managed by unique_ptr, destruction is automatic.
    _components.clear(); // Explicitly clear vector

    // Protocol and NIC are owned by Vehicle in this design
    delete _gateway;
}

void Vehicle::start() {
    db<Vehicle>(TRC) << "Vehicle::start() called for ID " << _id << "!\n";
    if (running()) {
        db<Vehicle>(WRN) << "[Vehicle " << _id << "] start() called but already running.\n";
        return;
    }

    _running.store(true, std::memory_order_release);

    db<Vehicle>(INF) << "[Vehicle " << _id << "] started.\n";
}

void Vehicle::stop() {
    db<Vehicle>(TRC) << "Vehicle::stop() called for ID " << _id << "!\n";

    if (!running()) {
        db<Vehicle>(WRN) << "[Vehicle " << _id << "] stop() called but not running.\n";
        return;
    }

    _running.store(false, std::memory_order_release); // Mark vehicle as stopped
}

template <typename ComponentType>
void Vehicle::create_component(const std::string& name) {
    _components.push_back(std::make_unique<ComponentType>(_gateway->bus(), name));   
}

const unsigned int Vehicle::id() const {
    return _id;
}

const bool Vehicle::running() const {
    return _running.load(std::memory_order_acquire);
}

#endif // VEHICLE_H