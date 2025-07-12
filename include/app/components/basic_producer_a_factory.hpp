#ifndef BASIC_PRODUCER_A_FACTORY_HPP
#define BASIC_PRODUCER_A_FACTORY_HPP

#include <memory>
#include <stdexcept>
#include "api/framework/agent.h"
#include "api/framework/component_functions.hpp"
#include "unit_a_data.hpp"
#include "unit_a_functions.hpp"
#include "../datatypes.h"

/**
 * @brief Creates a BasicProducerA agent using function-based composition
 * 
 * Replaces the inheritance-based BasicProducerA class with EPOS SmartData
 * principles. Eliminates vtable race conditions during destruction by using
 * function pointers instead of virtual methods.
 * 
 * This factory function creates a complete Agent configured as a producer
 * for UNIT_A data, with configurable random value generation range.
 * 
 * @param can CAN bus for communication (must not be null)
 * @param addr Network address for the agent
 * @param name Agent name for identification (default: "BasicProducerA")
 * @param min_range Minimum value for random generation (default: 0.0f)
 * @param max_range Maximum value for random generation (default: 100.0f)
 * @return Unique pointer to configured Agent ready for operation
 * @throws std::invalid_argument if can is null, name is empty, or invalid range
 */
inline std::unique_ptr<Agent> create_basic_producer_a(
    CAN* can, 
    const Agent::Address& addr, 
    const std::string& name = "BasicProducerA",
    float min_range = 0.0f,
    float max_range = 100.0f
) {
    // Parameter validation following EPOS principles
    if (!can) {
        throw std::invalid_argument("CAN bus cannot be null");
    }

    // Use default name if an empty string is provided
    const std::string& agentName = name.empty() ? "BasicProducerA" : name;

    if (min_range >= max_range) {
        throw std::invalid_argument("Invalid range: min_range must be < max_range");
    }
    
    // Create component data with specified range
    auto data = std::make_unique<UnitAData>();
    data->update_range(min_range, max_range);
    
    // Create Agent using function-based composition
    return std::make_unique<Agent>(
        can,                                                    // CAN bus
        agentName,                                              // Agent name
        static_cast<std::uint32_t>(DataTypes::UNIT_A),        // Data unit
        Agent::Type::INTEREST,                                 // Producer observes INTEREST
        addr,                                                  // Network address
        basic_producer_a,                                      // Producer function pointer
        nullptr,                                               // No response handler for producers
        std::move(data)                                        // Component data
    );
}

#endif // BASIC_PRODUCER_A_FACTORY_HPP 