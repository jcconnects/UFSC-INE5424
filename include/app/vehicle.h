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
#include "api/framework/clock.h"
#include "api/framework/leaderKeyStorage.h"
#include "api/network/ethernet.h"

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

    // Set self ID for the Clock instance
    // Assuming Gateway's address is the Vehicle's primary address for PTP ID purposes
    // The last byte of the MAC address is used as LeaderIdType
    LeaderIdType self_leader_id = static_cast<LeaderIdType>(_gateway->address().paddr().bytes[5]);
    if (self_leader_id != INVALID_LEADER_ID) {
        Clock::getInstance().setSelfId(self_leader_id);
        db<Vehicle>(INF) << "[Vehicle " << _id << "] registered self_id " << self_leader_id << " with Clock.\n";
        Clock::getInstance().activate(nullptr); // Activate clock to evaluate leader state
    } else {
        db<Vehicle>(WRN) << "[Vehicle " << _id << "] has an INVALID_LEADER_ID based on its Gateway MAC. Clock self_id not set.\n";
    }

    // HARDCODE: Set RSU as the leader in LeaderKeyStorage for vehicles
    // This matches the RSU_ID from the demo (1000)
    const unsigned int RSU_ID = 1000;
    Ethernet::Address rsu_mac;
    rsu_mac.bytes[0] = 0x02; // Locally administered (matches demo.cpp setup)
    rsu_mac.bytes[1] = 0x00;
    rsu_mac.bytes[2] = 0x00;
    rsu_mac.bytes[3] = 0x00;
    rsu_mac.bytes[4] = (RSU_ID >> 8) & 0xFF; // = 3 for RSU_ID 1000
    rsu_mac.bytes[5] = RSU_ID & 0xFF;        // = 232 for RSU_ID 1000
    
    auto& storage = LeaderKeyStorage::getInstance();
    storage.setLeaderId(rsu_mac);
    
    // Set a dummy MAC key for completeness (matches demo.cpp)
    MacKeyType rsu_key;
    rsu_key.fill(0);
    rsu_key[0] = (RSU_ID >> 8) & 0xFF;
    rsu_key[1] = RSU_ID & 0xFF;
    rsu_key[2] = 0xAA; // Marker for RSU
    rsu_key[3] = 0xBB;
    storage.setGroupMacKey(rsu_key);
    
    db<Vehicle>(INF) << "[Vehicle " << _id << "] set RSU " << RSU_ID << " as leader with MAC: " 
                     << Ethernet::mac_to_string(rsu_mac) << " (leader_id=" << static_cast<unsigned int>(rsu_mac.bytes[5]) << ")\n";
}

Vehicle::~Vehicle() {
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
    // CRITICAL FIX: Create unique address for each agent instead of using gateway address
    // This prevents the gateway from filtering out agent messages as "self-originated"
    static unsigned int component_counter = 1;
    Gateway::Address component_addr(_gateway->address().paddr(), component_counter++);
    
    auto component = std::make_unique<ComponentType>(_gateway->bus(), component_addr, name);
    
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