#ifndef INS_FACTORY_HPP
#define INS_FACTORY_HPP

#include <memory>
#include <stdexcept>
#include "../../api/framework/agent.h"
#include "ins_data.hpp"
#include "ins_functions.hpp"
#include "../datatypes.h"

/**
 * @brief Creates an INS (Inertial Navigation System) component using function-based composition
 * 
 * Replaces the inheritance-based INSComponent class with EPOS SmartData
 * principles. Eliminates vtable race conditions during destruction by using
 * function pointers instead of virtual methods.
 * 
 * INS components are producer-only and generate navigation data including
 * position, velocity, acceleration, gyroscope readings, and heading information.
 * This factory creates a complete Agent configured as a producer for EXTERNAL_INERTIAL_POSITION.
 * 
 * @param can CAN bus for communication (must not be null)
 * @param addr Network address for the agent
 * @param name Agent name for identification (default: "INSComponent")
 * @return Unique pointer to configured Agent ready for operation
 * @throws std::invalid_argument if can is null or name is empty
 */
inline std::unique_ptr<Agent> create_ins_component(
    CAN* can, 
    const Agent::Address& addr, 
    const std::string& name = "INSComponent"
) {
    // Parameter validation following EPOS principles
    if (!can) {
        throw std::invalid_argument("CAN bus cannot be null");
    }
    if (name.empty()) {
        throw std::invalid_argument("Agent name cannot be empty");
    }
    
    // Create component data for INS producer
    auto data = std::make_unique<INSData>();
    
    // Create Agent using function-based composition
    // INS is configured as producer for EXTERNAL_INERTIAL_POSITION (matching original)
    return std::make_unique<Agent>(
        can,                                                              // CAN bus
        name,                                                             // Agent name
        static_cast<std::uint32_t>(DataTypes::EXTERNAL_INERTIAL_POSITION), // Data unit (matching original)
        Agent::Type::INTEREST,                                            // Producer observes INTEREST
        addr,                                                             // Network address
        ins_producer,                                                     // Data generation function pointer
        ins_consumer,                                                     // Response handler (unused for producer)
        std::move(data)                                                   // Component data
    );
}

/**
 * @brief Creates an INS component with custom navigation ranges
 * 
 * Creates an INS component with customized position and motion ranges.
 * Useful for testing specific scenarios or configuring INS for different
 * operational environments.
 * 
 * @param can CAN bus for communication (must not be null)
 * @param addr Network address for the agent
 * @param x_min Minimum X coordinate (meters)
 * @param x_max Maximum X coordinate (meters)
 * @param y_min Minimum Y coordinate (meters)
 * @param y_max Maximum Y coordinate (meters)
 * @param alt_min Minimum altitude (meters)
 * @param alt_max Maximum altitude (meters)
 * @param name Agent name for identification (default: "INSComponent")
 * @return Unique pointer to configured Agent with custom ranges
 * @throws std::invalid_argument if can is null, name is empty, or ranges are invalid
 */
inline std::unique_ptr<Agent> create_ins_component_with_ranges(
    CAN* can, 
    const Agent::Address& addr,
    double x_min, double x_max,
    double y_min, double y_max,
    double alt_min, double alt_max,
    const std::string& name = "INSComponent"
) {
    // Parameter validation
    if (!can) {
        throw std::invalid_argument("CAN bus cannot be null");
    }
    if (name.empty()) {
        throw std::invalid_argument("Agent name cannot be empty");
    }
    if (x_min >= x_max || y_min >= y_max || alt_min >= alt_max) {
        throw std::invalid_argument("Invalid range parameters: min must be less than max");
    }
    
    // Create component data with custom ranges
    auto data = std::make_unique<INSData>();
    data->update_position_range(x_min, x_max, y_min, y_max, alt_min, alt_max);
    
    // Create Agent using function-based composition
    return std::make_unique<Agent>(
        can,                                                              // CAN bus
        name,                                                             // Agent name
        static_cast<std::uint32_t>(DataTypes::EXTERNAL_INERTIAL_POSITION), // Data unit
        Agent::Type::INTEREST,                                            // Producer observes INTEREST
        addr,                                                             // Network address
        ins_producer,                                                     // Data generation function pointer
        ins_consumer,                                                     // Response handler (unused)
        std::move(data)                                                   // Component data with custom ranges
    );
}

/**
 * @brief Creates an INS component with custom motion parameters
 * 
 * Creates an INS component with customized velocity and acceleration ranges.
 * Useful for simulating different vehicle types or motion profiles.
 * 
 * @param can CAN bus for communication (must not be null)
 * @param addr Network address for the agent
 * @param vel_min Minimum velocity (m/s)
 * @param vel_max Maximum velocity (m/s)
 * @param accel_min Minimum acceleration (m/s²)
 * @param accel_max Maximum acceleration (m/s²)
 * @param name Agent name for identification (default: "INSComponent")
 * @return Unique pointer to configured Agent with custom motion parameters
 * @throws std::invalid_argument if can is null, name is empty, or parameters are invalid
 */
inline std::unique_ptr<Agent> create_ins_component_with_motion(
    CAN* can, 
    const Agent::Address& addr,
    double vel_min, double vel_max,
    double accel_min, double accel_max,
    const std::string& name = "INSComponent"
) {
    // Parameter validation
    if (!can) {
        throw std::invalid_argument("CAN bus cannot be null");
    }
    if (name.empty()) {
        throw std::invalid_argument("Agent name cannot be empty");
    }
    if (vel_min >= vel_max || accel_min >= accel_max) {
        throw std::invalid_argument("Invalid motion parameters: min must be less than max");
    }
    
    // Create component data with custom motion parameters
    auto data = std::make_unique<INSData>();
    data->update_motion_range(vel_min, vel_max, accel_min, accel_max);
    
    // Create Agent using function-based composition
    return std::make_unique<Agent>(
        can,                                                              // CAN bus
        name,                                                             // Agent name
        static_cast<std::uint32_t>(DataTypes::EXTERNAL_INERTIAL_POSITION), // Data unit
        Agent::Type::INTEREST,                                            // Producer observes INTEREST
        addr,                                                             // Network address
        ins_producer,                                                     // Data generation function pointer
        ins_consumer,                                                     // Response handler (unused)
        std::move(data)                                                   // Component data with custom motion
    );
}

#endif // INS_FACTORY_HPP 