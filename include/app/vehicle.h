#ifndef VEHICLE_H
#define VEHICLE_H

#include <atomic> // for std::atomic
#include <vector> // for std::vector
#include <memory> // for std::unique_ptr
#include <string> // for std::string
#include "api/util/debug.h"
#include "api/util/csv_logger.h"
#include "api/framework/gateway.h"
#include "api/framework/agent.h"
#include "api/framework/clock.h"
#include "api/framework/leaderKeyStorage.h"
#include "api/network/ethernet.h"
#include "api/framework/vehicleRSUManager.h"

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
        
        // Expose RSU manager for testing/debugging
        VehicleRSUManager<Gateway::Protocol>* rsu_manager() const { return _rsu_manager.get(); }
        
        // Set transmission radius
        void setTransmissionRadius(double radius_m);
    private:
        unsigned int _id;
        Gateway* _gateway;
        std::atomic<bool> _running;
        std::vector<std::unique_ptr<Agent>> _components;
        std::string _log_dir;
        std::unique_ptr<VehicleRSUManager<Gateway::Protocol>> _rsu_manager;
};

/******** Vehicle Implementation *********/
inline Vehicle::Vehicle(unsigned int id) : _id(id), _running(false)
{
    _gateway = new Gateway(_id, Network::EntityType::VEHICLE);
    _rsu_manager = std::make_unique<VehicleRSUManager<Gateway::Protocol>>(_id);
    _gateway->network()->set_vehicle_rsu_manager(_rsu_manager.get());
    
    // Set up CSV logging directory
    _log_dir = CSVLogger::create_vehicle_log_dir(_id);
    setup_csv_logging();

    // Set self ID for the Clock instance (will be updated by RSU manager when leader is selected)
    LeaderIdType self_leader_id = static_cast<LeaderIdType>(_gateway->address().paddr().bytes[5]);
    if (self_leader_id != INVALID_LEADER_ID) {
        Clock::getInstance().setSelfId(self_leader_id);
        db<Vehicle>(INF) << "[Vehicle " << _id << "] registered self_id " << self_leader_id << " with Clock.\n";
        Clock::getInstance().activate(nullptr);
    }
    db<Vehicle>(INF) << "[Vehicle " << _id << "] initialized with RSU management\n";
}

inline Vehicle::~Vehicle() {
    // Ensure vehicle is stopped first
    if (running()) {
        stop();
    }

    // Components are managed by unique_ptr, destruction is automatic.
    // But we clear them explicitly to ensure proper destruction order
    _components.clear();

    // Protocol and NIC are owned by Vehicle in this design
    db<Vehicle>(TRC) << "Vehicle::~Vehicle() called for ID " << _id << "!\n";
    delete _gateway;
    db<Vehicle>(INF) << "[Vehicle " << _id << "] destroyed successfully.\n";
}

inline void Vehicle::start() {
    db<Vehicle>(TRC) << "Vehicle::start() called for ID " << _id << "!\n";
    if (running()) {
        db<Vehicle>(WRN) << "[Vehicle " << _id << "] start() called but already running.\n";
        return;
    }

    _running.store(true, std::memory_order_release);

    db<Vehicle>(INF) << "[Vehicle " << _id << "] started.\n";
}

inline void Vehicle::stop() {
    db<Vehicle>(TRC) << "Vehicle::stop() called for ID " << _id << "!\n";

    if (!running()) {
        db<Vehicle>(WRN) << "[Vehicle " << _id << "] stop() called but not running.\n";
        return;
    }

    _running.store(false, std::memory_order_release); // Mark vehicle as stopped
    db<Vehicle>(INF) << "[Vehicle " << _id << "] stopped.\n";
}

template <typename ComponentType>
inline void Vehicle::create_component(const std::string& name) {
    // CRITICAL FIX: Create unique address for each agent instead of using gateway address
    // This prevents the gateway from filtering out agent messages as "self-originated"
    static unsigned int component_counter = 1;
    Gateway::Address component_addr(_gateway->address().paddr(), component_counter++);
    
    auto component = std::make_unique<ComponentType>(_gateway->bus(), component_addr, name);
    
    // Set up CSV logging for the component
    component->set_csv_logger(_log_dir);
    
    _components.push_back(std::move(component));   
}

inline const unsigned int Vehicle::id() const {
    return _id;
}

inline const bool Vehicle::running() const {
    return _running.load(std::memory_order_acquire);
}

// CSV logging setup
inline void Vehicle::setup_csv_logging() {
    // Set up CSV logging for the gateway
    _gateway->setup_csv_logging(_log_dir);
}

// Set transmission radius
inline void Vehicle::setTransmissionRadius(double radius_m) {
    _gateway->network()->channel()->setRadius(radius_m);
    db<Vehicle>(INF) << "[Vehicle " << _id << "] transmission radius set to " << radius_m << "m\n";
}

#endif // VEHICLE_H