#ifndef VEHICLE_H
#define VEHICLE_H

#include <atomic> // for std::atomic
#include <vector> // for std::vector
#include <memory> // for std::unique_ptr
#include <chrono> // for timeouts
#include <map> // for component type mapping

#include "debug.h"
#include "initializer.h"
#include "component.h"
#include "teds.h" // For DataTypeId

// Forward declarations
class BasicProducer;
class BasicConsumer;
class GatewayComponent;

template <typename Engine1, typename Engine2>
class NIC;

template <typename NIC>
class Protocol;

class SocketEngine;

class SharedMemoryEngine;

// Vehicle class definition
class Vehicle {

    public:
        typedef NIC<SocketEngine, SharedMemoryEngine> VehicleNIC;
        typedef Protocol<VehicleNIC> VehicleProt;
        typedef VehicleNIC::Address Address;

        // Defining component ports (expanded to include our test components)
        enum class Ports {
            GATEWAY = 0,        // Gateway is always port 0
            BASIC_PRODUCER = 105, // Basic producer port from basic_producer.h
            BASIC_CONSUMER = 106, // Basic consumer port from basic_consumer.h
            // Legacy ports kept for compatibility
            BROADCAST,
            ECU1,
            ECU2,
            BATTERY,
            INS,
            LIDAR,
            CAMERA
        };

        // Vehicle constructor
        Vehicle(unsigned int id);

        ~Vehicle();

        const unsigned int id() const;
        const bool running() const;

        void start();
        void stop();

        template <typename ComponentType, typename... Args>
        void create_component(const std::string& name, Args&&... args);
        void start_components();
        void stop_components();

        // Simplified component management
        void start_component(const std::string& component_name);
        Component* get_component(const std::string& name);

        VehicleProt* protocol() const;
        
        const Address address() const;

        // Returns mapping of data types to producer components for hardcoded configurations
        static std::map<DataTypeId, Ports> get_producer_port_map() {
            std::map<DataTypeId, Ports> map;
            // Add known producer mappings
            map[DataTypeId::CUSTOM_SENSOR_DATA_A] = Ports::BASIC_PRODUCER;
            return map;
        }
        
        // Instance method to access the static mapping
        std::map<DataTypeId, Ports> get_producer_ports() const {
            return get_producer_port_map();
        }

    private:
        unsigned int _id;

        // Update member types
        VehicleProt* _protocol;
        VehicleNIC* _nic;

        std::atomic<bool> _running;
        std::vector<std::unique_ptr<Component>> _components;
};

/******** Vehicle Implementation *********/
Vehicle::Vehicle(unsigned int id) : _id(id), _running(false)
{
    db<Vehicle>(TRC) << "Vehicle::Vehicle() called!\n";

    // Setting vehicle NIC
    _nic = Initializer::create_nic();
    
    // Setting NIC address
    Address addr; // We don't set the address here anymore;
    addr.bytes[0] = 0x02; // the NIC gets its address from the SocketEngine.
    addr.bytes[1] = 0x00;
    addr.bytes[2] = 0x00;
    addr.bytes[3] = 0x00;
    addr.bytes[4] = (id >> 8) & 0xFF;
    addr.bytes[5] = id & 0xFF;
    _nic->setAddress(addr);
    
    // Setting vehicle protocol
    _protocol = Initializer::create_protocol(_nic);
    
     db<Vehicle>(INF) << "[Vehicle " << _id << "] created with address: " << VehicleNIC::mac_to_string(address()) << "\n";
}

Vehicle::~Vehicle() {
    db<Vehicle>(TRC) << "Vehicle::~Vehicle() called for ID " << _id << "!\n";

    // Ensure components and NIC/Protocol are stopped before deletion
    // Explicit stop() might be called externally, but ensure it happens.
    if (running()) { // Check if running before attempting stop
        stop();
    }

    // Components are managed by unique_ptr, destruction is automatic.
    _components.clear(); // Explicitly clear vector

    // Protocol and NIC are owned by Vehicle in this design
    delete _protocol; // Protocol should be deleted before NIC
    delete _nic;
    db<Vehicle>(INF) << "[Vehicle " << _id << "] Protocol and NIC deleted.\n";
}

void Vehicle::start() {
    db<Vehicle>(TRC) << "Vehicle::start() called for ID " << _id << "!\n";
    if (running()) {
        db<Vehicle>(WRN) << "[Vehicle " << _id << "] start() called but already running.\n";
        return;
    }

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

    // First stop NIC and its engines
    _nic->stop();

    // Then stops each component
    db<Vehicle>(INF) << "[Vehicle " << _id << "] Stopping components...\n";
    stop_components();

    _running.store(false, std::memory_order_release); // Mark vehicle as stopped
     db<Vehicle>(INF) << "[Vehicle " << _id << "] stopped.\n";
}

template <typename ComponentType, typename... Args>
void Vehicle::create_component(const std::string& name, Args&&... args) {
    _components.push_back(std::make_unique<ComponentType>(this, id(), name, protocol(), std::forward<Args>(args)...));   
}

void Vehicle::start_components() {
    db<Vehicle>(TRC) << "Vehicle::start_components() called for ID " << _id << "!\n";
    if (_components.empty()) {
         db<Vehicle>(INF) << "[Vehicle " << _id << "] No components to start.\n";
         return;
    }

    db<Vehicle>(INF) << "[Vehicle " << _id << "] Starting " << _components.size() << " components...\n";
    for (const auto& c : _components) {
        c->start(); // Component::start() creates the thread
        db<Vehicle>(INF) << "[Vehicle " << _id << "] component " << c->getName() << " started\n";
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
    for (const auto& c: _components) {
        c->stop(); // Component::stop() signals and joins the thread
        db<Vehicle>(TRC) << "[Vehicle " << _id << "] component " << c->getName() << " stopped.\n";
    }
    db<Vehicle>(INF) << "[Vehicle " << _id << "] All components stopped.\n";
}

const unsigned int Vehicle::id() const {
    return _id;
}

const bool Vehicle::running() const {
    return _running.load(std::memory_order_acquire);
}

Vehicle::VehicleProt* Vehicle::protocol() const {
    return _protocol;
}

const Vehicle::Address Vehicle::address() const {
    return _nic->address();
}

// Add simplified method for component management
void Vehicle::start_component(const std::string& component_name) {
    for (auto& comp : _components) {
        if (comp->getName() == component_name) {
            if (!comp->running()) {
                comp->start();
                db<Vehicle>(INF) << "[Vehicle " << _id << "] component " << comp->getName() << " started\n";
            } else {
                db<Vehicle>(WRN) << "[Vehicle " << _id << "] component " << comp->getName() << " already running\n";
            }
            return;
        }
    }
    db<Vehicle>(ERR) << "[Vehicle " << _id << "] component " << component_name << " not found\n";
}

Component* Vehicle::get_component(const std::string& name) {
    for (auto& comp : _components) {
        if (comp->getName() == name) {
            return comp.get();
        }
    }
    return nullptr;
}

// Include component headers at the end to avoid circular dependencies
#include "components/basic_producer.h"
#include "components/basic_consumer.h"
#include "components/gateway_component.h"

#endif // VEHICLE_H