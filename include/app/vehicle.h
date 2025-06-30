#ifndef VEHICLE_H
#define VEHICLE_H

#include <atomic> // for std::atomic
#include <csignal>
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

// Include all component factory headers for Phase 4.3
#include "components/camera_factory.hpp"
#include "components/lidar_factory.hpp"
#include "components/ecu_factory.hpp"
#include "components/ins_factory.hpp"
#include "components/basic_producer_a_factory.hpp"
#include "components/basic_producer_b_factory.hpp"
#include "components/basic_consumer_a_factory.hpp"
#include "components/basic_consumer_b_factory.hpp"
#include "components/csv_component_factory.hpp"
#include "components/csv_consumer_factory.hpp"

// Component type identifiers for template specialization
// These replace the old inheritance-based component classes
struct ECUComponent {};
struct CameraComponent {};
struct LidarComponent {};
struct INSComponent {};
struct BasicProducerA {};
struct BasicProducerB {};
struct BasicConsumerA {};
struct BasicConsumerB {};
struct CSVComponent {};
struct CSVConsumerComponent {};

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
        
        template <typename ComponentType>
        ComponentType* get_component(const std::string& name);
        
        // Special method for CSV component creation with file path
        void create_csv_component_with_file(const std::string& name, const std::string& csv_file_path);
        
        // CSV logging setup
        void setup_csv_logging();
        
        // Expose RSU manager for testing/debugging
        VehicleRSUManager<Gateway::Protocol>* rsu_manager() const { return _rsu_manager.get(); }
        
        // Set transmission radius
        void setTransmissionRadius(double radius_m);
        
        /**
         * @brief Get the number of components in this vehicle
         * 
         * @return The number of components currently attached to this vehicle
         */
        size_t component_count() const;
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
    _gateway->start();

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

/**
 * @brief Generic template with compile-time error for unsupported types
 * 
 * This template provides a compile-time error message for unsupported component types.
 * Only the specialized templates below should be used for actual component creation.
 * Following EPOS principles of compile-time type safety.
 */
template <typename ComponentType>
inline void Vehicle::create_component(const std::string& name) {
    static_assert(sizeof(ComponentType) == 0, 
        "Unsupported component type. Supported types: ECUComponent, CameraComponent, LidarComponent, INSComponent, BasicProducerA, BasicProducerB, BasicConsumerA, BasicConsumerB, CSVComponent, CSVConsumerComponent");
}

/**
 * @brief Template specialization for ECU component creation
 * 
 * Creates an ECU component using the factory function instead of direct instantiation.
 * This eliminates the inheritance-based approach and uses function-based composition.
 * 
 * @param name Component name for identification
 */
template<>
inline void Vehicle::create_component<ECUComponent>(const std::string& name) {
    static unsigned int component_counter = 1;
    Gateway::Address component_addr(_gateway->address().paddr(), component_counter++);
    
    auto component = create_ecu_component(_gateway->bus(), component_addr, name);
    component->set_csv_logger(_log_dir);
    _components.push_back(std::move(component));
}

/**
 * @brief Template specialization for Camera component creation
 * 
 * Creates a Camera component using the factory function instead of direct instantiation.
 * This eliminates the inheritance-based approach and uses function-based composition.
 * 
 * @param name Component name for identification
 */
template<>
inline void Vehicle::create_component<CameraComponent>(const std::string& name) {
    static unsigned int component_counter = 1;
    Gateway::Address component_addr(_gateway->address().paddr(), component_counter++);
    
    auto component = create_camera_component(_gateway->bus(), component_addr, name);
    component->set_csv_logger(_log_dir);
    _components.push_back(std::move(component));
}

/**
 * @brief Template specialization for Lidar component creation
 * 
 * Creates a Lidar component using the factory function instead of direct instantiation.
 * This eliminates the inheritance-based approach and uses function-based composition.
 * 
 * @param name Component name for identification
 */
template<>
inline void Vehicle::create_component<LidarComponent>(const std::string& name) {
    static unsigned int component_counter = 1;
    Gateway::Address component_addr(_gateway->address().paddr(), component_counter++);
    
    auto component = create_lidar_component(_gateway->bus(), component_addr, name);
    component->set_csv_logger(_log_dir);
    _components.push_back(std::move(component));
}

/**
 * @brief Template specialization for INS component creation
 * 
 * Creates an INS component using the factory function instead of direct instantiation.
 * This eliminates the inheritance-based approach and uses function-based composition.
 * 
 * @param name Component name for identification
 */
template<>
inline void Vehicle::create_component<INSComponent>(const std::string& name) {
    static unsigned int component_counter = 1;
    Gateway::Address component_addr(_gateway->address().paddr(), component_counter++);
    
    auto component = create_ins_component(_gateway->bus(), component_addr, name);
    component->set_csv_logger(_log_dir);
    _components.push_back(std::move(component));
}

/**
 * @brief Template specialization for BasicProducerA component creation
 * 
 * Creates a BasicProducerA component using the factory function instead of direct instantiation.
 * This eliminates the inheritance-based approach and uses function-based composition.
 * 
 * @param name Component name for identification
 */
template<>
inline void Vehicle::create_component<BasicProducerA>(const std::string& name) {
    static unsigned int component_counter = 1;
    Gateway::Address component_addr(_gateway->address().paddr(), component_counter++);
    
    auto component = create_basic_producer_a(_gateway->bus(), component_addr, name);
    component->set_csv_logger(_log_dir);
    _components.push_back(std::move(component));
}

/**
 * @brief Template specialization for BasicProducerB component creation
 * 
 * Creates a BasicProducerB component using the factory function instead of direct instantiation.
 * This eliminates the inheritance-based approach and uses function-based composition.
 * 
 * @param name Component name for identification
 */
template<>
inline void Vehicle::create_component<BasicProducerB>(const std::string& name) {
    static unsigned int component_counter = 1;
    Gateway::Address component_addr(_gateway->address().paddr(), component_counter++);
    
    auto component = create_basic_producer_b(_gateway->bus(), component_addr, name);
    component->set_csv_logger(_log_dir);
    _components.push_back(std::move(component));
}

/**
 * @brief Template specialization for BasicConsumerA component creation
 * 
 * Creates a BasicConsumerA component using the factory function instead of direct instantiation.
 * This eliminates the inheritance-based approach and uses function-based composition.
 * 
 * @param name Component name for identification
 */
template<>
inline void Vehicle::create_component<BasicConsumerA>(const std::string& name) {
    static unsigned int component_counter = 1;
    Gateway::Address component_addr(_gateway->address().paddr(), component_counter++);
    
    auto component = create_basic_consumer_a(_gateway->bus(), component_addr, name);
    component->set_csv_logger(_log_dir);
    _components.push_back(std::move(component));
}

/**
 * @brief Template specialization for BasicConsumerB component creation
 * 
 * Creates a BasicConsumerB component using the factory function instead of direct instantiation.
 * This eliminates the inheritance-based approach and uses function-based composition.
 * 
 * @param name Component name for identification
 */
template<>
inline void Vehicle::create_component<BasicConsumerB>(const std::string& name) {
    static unsigned int component_counter = 1;
    Gateway::Address component_addr(_gateway->address().paddr(), component_counter++);
    
    auto component = create_basic_consumer_b(_gateway->bus(), component_addr, name);
    component->set_csv_logger(_log_dir);
    _components.push_back(std::move(component));
}

/**
 * @brief Template specialization for CSV component creation
 * 
 * CSVComponent requires a CSV file path parameter that cannot be provided through
 * the standard template interface. This template specialization provides a clear
 * compile-time error directing users to the appropriate creation method.
 * 
 * @param name Component name for identification
 */
template<>
inline void Vehicle::create_component<CSVComponent>(const std::string& name) {
    throw std::runtime_error("CSVComponent requires a CSV file path. Use this method instead:\n"
                             "- create_csv_component_with_file(name, csv_file_path)");
}

/**
 * @brief Template specialization for CSV consumer component creation
 * 
 * Creates a CSV consumer component using the factory function instead of direct instantiation.
 * This eliminates the inheritance-based approach and uses function-based composition.
 * 
 * @param name Component name for identification
 */
template<>
inline void Vehicle::create_component<CSVConsumerComponent>(const std::string& name) {
    static unsigned int component_counter = 1;
    Gateway::Address component_addr(_gateway->address().paddr(), component_counter++);
    
    auto component = create_csv_consumer(_gateway->bus(), component_addr, name);
    component->set_csv_logger(_log_dir);
    _components.push_back(std::move(component));
}


/**
 * @brief Create CSV component with specific file path
 * 
 * Creates a CSV component using the provided CSV file path.
 * This method handles the file path parameter that the template version cannot.
 * 
 * @param name Component name for identification
 * @param csv_file_path Path to the CSV file to load
 */
inline void Vehicle::create_csv_component_with_file(const std::string& name, const std::string& csv_file_path) {
    static unsigned int component_counter = 1;
    Gateway::Address component_addr(_gateway->address().paddr(), component_counter++);
    
    auto component = create_csv_component(_gateway->bus(), component_addr, csv_file_path, name);
    component->set_csv_logger(_log_dir);
    _components.push_back(std::move(component));
}

template <typename ComponentType>
inline ComponentType* Vehicle::get_component(const std::string& name) {
    for (auto& component : _components) {
        if (component->name() == name) {
            return static_cast<ComponentType*>(component.get());
        }
    }
    return nullptr;
}

/**
 * @brief Template specialization for Agent component retrieval
 * 
 * Returns the Agent pointer directly without casting, since all components
 * are stored as Agent instances in the factory-based approach.
 * 
 * @param name Component name for identification
 * @return Pointer to Agent component or nullptr if not found
 */
template<>
inline Agent* Vehicle::get_component<Agent>(const std::string& name) {
    for (auto& component : _components) {
        if (component->name() == name) {
            return component.get();
        }
    }
    return nullptr;
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

/**
 * @brief Get the number of components in this vehicle
 * 
 * @return The number of components currently attached to this vehicle
 */
inline size_t Vehicle::component_count() const {
    return _components.size();
}

#endif // VEHICLE_H