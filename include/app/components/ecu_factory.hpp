#ifndef ECU_FACTORY_HPP
#define ECU_FACTORY_HPP

#include <memory>
#include <stdexcept>
#include "../../api/framework/agent.h"
#include "ecu_data.hpp"
#include "ecu_functions.hpp"
#include "../datatypes.h"

/**
 * @brief Creates an ECU component using function-based composition
 * 
 * Replaces the inheritance-based ECUComponent class with EPOS SmartData
 * principles. Eliminates vtable race conditions during destruction by using
 * function pointers instead of virtual methods.
 * 
 * ECU components are consumer-only and receive messages from other components
 * (Camera, Lidar, INS) for processing and control decisions. This factory
 * creates a complete Agent configured as a consumer for EXTERNAL_POINT_CLOUD_XYZ.
 * 
 * @param can CAN bus for communication (must not be null)
 * @param addr Network address for the agent
 * @param name Agent name for identification (default: "ECUComponent")
 * @return Unique pointer to configured Agent ready for operation
 * @throws std::invalid_argument if can is null or name is empty
 */
inline std::unique_ptr<Agent> create_ecu_component(
    CAN* can, 
    const Agent::Address& addr, 
    const std::string& name = "ECUComponent"
) {
    // Parameter validation following EPOS principles
    if (!can) {
        throw std::invalid_argument("CAN bus cannot be null");
    }

    // Use default name if an empty string is provided
    const std::string& agentName = name.empty() ? "ECUComponent" : name;
    
    // Create component data for ECU consumer
    auto data = std::make_unique<ECUData>();
    
    // Create Agent using function-based composition
    // ECU is configured as consumer for EXTERNAL_POINT_CLOUD_XYZ (matching original)
    return std::make_unique<Agent>(
        can,                                                           // CAN bus
        agentName,                                                     // Agent name
        static_cast<std::uint32_t>(DataTypes::EXTERNAL_POINT_CLOUD_XYZ), // Data unit (matching original)
        Agent::Type::RESPONSE,                                         // Consumer observes RESPONSE
        addr,                                                          // Network address
        ecu_producer,                                                  // Producer function (returns empty)
        ecu_consumer,                                                  // Response handler function pointer
        std::move(data)                                                // Component data
    );
}

/**
 * @brief Convenience function to create ECU and start consuming immediately
 * 
 * Creates an ECU component and immediately starts periodic interest for
 * receiving messages from producers. Useful for scenarios where ECU should
 * start consuming data immediately upon creation.
 * 
 * @param can CAN bus for communication (must not be null)
 * @param addr Network address for the agent
 * @param period Desired response period from producers (default: 1 second)
 * @param name Agent name for identification (default: "ECUComponent")
 * @return Unique pointer to configured Agent with periodic interest started
 * @throws std::invalid_argument if can is null or name is empty
 */
inline std::unique_ptr<Agent> create_ecu_component_with_period(
    CAN* can, 
    const Agent::Address& addr, 
    Agent::Microseconds period = Agent::Microseconds(1000000),
    const std::string& name = "ECUComponent"
) {
    auto ecu = create_ecu_component(can, addr, name);
    ecu->start_periodic_interest(static_cast<std::uint32_t>(DataTypes::EXTERNAL_POINT_CLOUD_XYZ), period);
    return ecu;
}

#endif // ECU_FACTORY_HPP 