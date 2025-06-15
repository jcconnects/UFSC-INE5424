#ifndef BASIC_PRODUCER_B_FACTORY_HPP
#define BASIC_PRODUCER_B_FACTORY_HPP

#include <memory>
#include <stdexcept>
#include "../../api/framework/agent_v2.hpp"
#include "../../api/framework/component_functions.hpp"
#include "unit_b_data.hpp"
#include "unit_b_functions.hpp"
#include "../datatypes.h"

/**
 * @brief Creates a BasicProducerB agent using function-based composition
 * 
 * Replaces the inheritance-based BasicProducerB class with EPOS SmartData
 * principles. Eliminates vtable race conditions during destruction by using
 * function pointers instead of virtual methods.
 * 
 * This factory function creates a complete Agent configured as a producer
 * for UNIT_B data, with configurable random value generation range.
 * 
 * @param can CAN bus for communication (must not be null)
 * @param addr Network address for the agent
 * @param name Agent name for identification (default: "BasicProducerB")
 * @param min_range Minimum value for random generation (default: 200.0f)
 * @param max_range Maximum value for random generation (default: 300.0f)
 * @return Unique pointer to configured Agent ready for operation
 * @throws std::invalid_argument if can is null, name is empty, or invalid range
 */
inline std::unique_ptr<Agent> create_basic_producer_b(
    CAN* can, 
    const Agent::Address& addr, 
    const std::string& name = "BasicProducerB",
    float min_range = 200.0f,
    float max_range = 300.0f
) {
    // Parameter validation following EPOS principles
    if (!can) {
        throw std::invalid_argument("CAN bus cannot be null");
    }
    if (name.empty()) {
        throw std::invalid_argument("Agent name cannot be empty");
    }
    if (min_range >= max_range) {
        throw std::invalid_argument("Invalid range: min_range must be < max_range");
    }
    
    // Create component data with specified range
    auto data = std::make_unique<UnitBData>(min_range, max_range);
    
    // Create Agent using function-based composition
    return std::make_unique<Agent>(
        can,                                                    // CAN bus
        name,                                                   // Agent name
        static_cast<std::uint32_t>(DataTypes::UNIT_B),        // Data unit
        Agent::Type::INTEREST,                                 // Producer observes INTEREST
        addr,                                                  // Network address
        basic_producer_b,                                      // Producer function pointer
        nullptr,                                               // No response handler for producers
        std::move(data)                                        // Component data
    );
}

#endif // BASIC_PRODUCER_B_FACTORY_HPP 