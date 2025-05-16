#ifndef VEHICLE_H
#define VEHICLE_H

#include <atomic> // for std::atomic
#include <vector> // for std::vector
#include <memory> // for std::unique_ptr
#include <chrono> // for timeouts
#include <map> // for component type mapping

#include "debug.h"
#include "initializer.h"
#include "teds.h" // For DataTypeId

// Forward declarations 
class Component;
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

        // Defining component ports
        enum class Ports {
            GATEWAY = 0,        // Gateway is always port 0
            INTERNAL_BROADCAST = 1, // Internal broadcast is port 1
            MIN_COMPONENT_PORT = 2, // Regular components start at port 2
            BASIC_PRODUCER = 105,
            BASIC_CONSUMER = 106
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

        // Get all component addresses (excluding Gateway)
        std::vector<Address> get_all_component_addresses() const;

    private:
        unsigned int _id;
        VehicleProt* _protocol;
        VehicleNIC* _nic;
        std::atomic<bool> _running;
        std::vector<std::unique_ptr<Component>> _components;
};

// Include component.h after all the forward declarations
#include "component.h"

/******** Vehicle Implementation *********/
Vehicle::Vehicle(unsigned int id) : _id(id), _running(false)
{
    db<Vehicle>(TRC) << "[Vehicle] Constructor called!\n";

    // Setting vehicle NIC
    _nic = Initializer::create_nic();
    
    // Setting NIC address
    Address addr;
    addr.bytes[0] = 0x02;
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
    db<Vehicle>(TRC) << "[Vehicle] Destructor called for ID " << _id << "!\n";

    // Ensure components and NIC/Protocol are stopped before deletion
    if (running()) {
        stop();
    }

    db<Vehicle>(INF) << "[Vehicle " << _id << "] Stopped components.\n";

    // Components are managed by unique_ptr, destruction is automatic.
    _components.clear();

    // Protocol and NIC are owned by Vehicle in this design
    delete _protocol;
    delete _nic;
    db<Vehicle>(INF) << "[Vehicle " << _id << "] Protocol and NIC deleted.\n";
}

void Vehicle::start() {
    db<Vehicle>(TRC) << "[Vehicle] start() called for ID " << _id << "!\n";
    if (running()) {
        db<Vehicle>(WRN) << "[Vehicle " << _id << "] start() called but already running.\n";
        return;
    }

    _running.store(true, std::memory_order_release);
    start_components();

    db<Vehicle>(INF) << "[Vehicle " << _id << "] started.\n";
}

void Vehicle::stop() {
    db<Vehicle>(TRC) << "[Vehicle] stop() called for ID " << _id << "!\n";

    if (!running()) {
        db<Vehicle>(WRN) << "[Vehicle " << _id << "] stop() called but not running.\n";
        return;
    }

    // First stop NIC and its engines
    _nic->stop();

    // Then stops each component
    db<Vehicle>(INF) << "[Vehicle] [" << _id << "] Stopping components...\n";
    stop_components();

    _running.store(false, std::memory_order_release);
    db<Vehicle>(INF) << "[Vehicle] [" << _id << "] stopped.\n";
}

template <typename ComponentType, typename... Args>
void Vehicle::create_component(const std::string& name, Args&&... args) {
    std::unique_ptr<Component> component = std::make_unique<ComponentType>(this, id(), name, protocol(), std::forward<Args>(args)...);
    _components.push_back(std::move(component));
}

void Vehicle::start_components() {
    db<Vehicle>(TRC) << "[Vehicle] start_components() called for ID " << _id << "!\n";
    if (_components.empty()) {
         db<Vehicle>(INF) << "[Vehicle] [" << _id << "] No components to start.\n";
         return;
    }

    db<Vehicle>(INF) << "[Vehicle] [" << _id << "] Starting " << _components.size() << " components in staged order...\n";
    
    // Step 1: Start the gateway component first if present
    for (const auto& c : _components) {
        if (c->type() == ComponentType::GATEWAY) {
            c->start();
            db<Vehicle>(INF) << "[Vehicle] [" << _id << "] Gateway component " << c->getName() << " started first\n";
            
            // Give Gateway time to initialize
            usleep(50000); // 50ms
            break;
        }
    }
    
    // Step 2: Start producer components second
    for (const auto& c : _components) {
        if (c->type() == ComponentType::PRODUCER && !c->running()) {
            c->start();
            db<Vehicle>(INF) << "[Vehicle] [" << _id << "] Producer component " << c->getName() << " started\n";
        }
    }
    
    // Give Producers time to start and be ready to receive interests
    usleep(100000); // 100ms
    
    // Step 3: Finally start consumer components
    for (const auto& c : _components) {
        if (c->type() == ComponentType::CONSUMER && !c->running()) {
            c->start();
            db<Vehicle>(INF) << "[Vehicle] [" << _id << "] Consumer component " << c->getName() << " started\n";
        }
    }
    
    db<Vehicle>(INF) << "[Vehicle] [" << _id << "] All components started in staged sequence.\n";
}

void Vehicle::stop_components() {
    db<Vehicle>(TRC) << "[Vehicle] stop_components() called for ID " << _id << "!\n";
    if (_components.empty()) {
         db<Vehicle>(INF) << "[Vehicle] [" << _id << "] No components to stop.\n";
         return;
    }
    
    // Stop components in reverse order: first regular components, then gateway
    db<Vehicle>(INF) << "[Vehicle] [" << _id << "] Stopping " << _components.size() << " components...\n";
    
    // First stop non-gateway components
    for (const auto& c: _components) {
        if (c->type() != ComponentType::GATEWAY && c->running()) {
            c->stop();
            db<Vehicle>(TRC) << "[Vehicle] [" << _id << "] component " << c->getName() << " stopped.\n";
        }
    }
    
    // Then stop gateway component
    for (const auto& c: _components) {
        if (c->type() == ComponentType::GATEWAY && c->running()) {
            c->stop();
            db<Vehicle>(TRC) << "[Vehicle] [" << _id << "] Gateway component " << c->getName() << " stopped.\n";
            break;
        }
    }
    
    db<Vehicle>(INF) << "[Vehicle] [" << _id << "] All components stopped.\n";
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

void Vehicle::start_component(const std::string& component_name) {
    for (auto& comp : _components) {
        if (comp->getName() == component_name) {
            if (!comp->running()) {
                comp->start();
                db<Vehicle>(INF) << "[Vehicle] [" << _id << "] component " << comp->getName() << " started\n";
            } else {
                db<Vehicle>(WRN) << "[Vehicle] [" << _id << "] component " << comp->getName() << " already running\n";
            }
            return;
        }
    }
    db<Vehicle>(ERR) << "[Vehicle] [" << _id << "] component " << component_name << " not found\n";
}

Component* Vehicle::get_component(const std::string& name) {
    for (auto& comp : _components) {
        if (comp->getName() == name) {
            return comp.get();
        }
    }
    return nullptr;
}

std::vector<Vehicle::Address> Vehicle::get_all_component_addresses() const {
    std::vector<Address> addresses;
    for (const auto& comp_ptr : _components) {
        if (comp_ptr) { 
            // Exclude the Gateway's own port
            if (comp_ptr->address().port() != Component::GATEWAY_PORT) {
                 // Extract just the MAC address part from the component address
                 addresses.push_back(comp_ptr->address().paddr());
            }
        }
    }
    return addresses;
}

// Include component headers after implementation
#include "components/basic_producer.h"
#include "components/basic_consumer.h"
#include "components/gateway_component.h"

#endif // VEHICLE_H