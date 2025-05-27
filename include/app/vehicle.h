#ifndef VEHICLE_H
#define VEHICLE_H

#include <atomic> // for std::atomic
#include <vector> // for std::vector
#include <memory> // for std::unique_ptr
#include <thread> // for std::this_thread
#include <chrono> // for std::chrono
#include <type_traits> // for std::is_same_v

#include "api/util/debug.h"
#include "api/util/csv_logger.h"
#include "api/framework/gateway.h"
#include "api/framework/agent.h"

// Forward declarations
class ECUComponent;

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
        
        // CSV logging setup
        void setup_csv_logging();
        
    private:
        unsigned int _id;
        Gateway* _gateway;
        std::atomic<bool> _running;
        std::vector<std::unique_ptr<Agent>> _components;
        std::string _log_dir;
};

/******** Vehicle Implementation *********/
Vehicle::Vehicle(unsigned int id) : _id(id), _running(false)
{
    _gateway = new Gateway(_id);
    
    // Set up CSV logging directory
    _log_dir = CSVLogger::create_vehicle_log_dir(_id);
    setup_csv_logging();
}

Vehicle::~Vehicle() {
    // Ensure vehicle is stopped first
    if (running()) {
        stop();
    }

    // Give components time to finish their current operations
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Components are managed by unique_ptr, destruction is automatic.
    // But we clear them explicitly to ensure proper destruction order
    _components.clear();

    // Protocol and NIC are owned by Vehicle in this design
    db<Vehicle>(TRC) << "Vehicle::~Vehicle() called for ID " << _id << "!\n";
    delete _gateway;
    db<Vehicle>(INF) << "[Vehicle " << _id << "] destroyed successfully.\n";
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
    db<Vehicle>(INF) << "[Vehicle " << _id << "] stopped.\n";
}

template <typename ComponentType>
void Vehicle::create_component(const std::string& name) {
    auto component = std::make_unique<ComponentType>(_gateway->bus(), _gateway->address(), name);
    
    // Set up CSV logging for the component
    component->set_csv_logger(_log_dir);
    
    _components.push_back(std::move(component));   
}

const unsigned int Vehicle::id() const {
    return _id;
}

const bool Vehicle::running() const {
    return _running.load(std::memory_order_acquire);
}

// CSV logging setup
void Vehicle::setup_csv_logging() {
    // Set up CSV logging for the gateway
    _gateway->setup_csv_logging(_log_dir);
}

#endif // VEHICLE_H