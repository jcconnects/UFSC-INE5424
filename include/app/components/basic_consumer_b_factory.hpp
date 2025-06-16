#ifndef BASIC_CONSUMER_B_FACTORY_HPP
#define BASIC_CONSUMER_B_FACTORY_HPP

#include <memory>
#include <stdexcept>
#include "api/framework/agent.h"
#include "../../api/framework/component_functions.hpp"
#include "unit_b_data.hpp"
#include "unit_b_functions.hpp"
#include "../datatypes.h"

/**
 * @brief Creates a BasicConsumerB agent using function-based composition
 * 
 * Replaces the inheritance-based BasicConsumerB class with EPOS SmartData
 * principles. Eliminates vtable race conditions during destruction by using
 * function pointers instead of virtual methods.
 * 
 * This factory function creates a complete Agent configured as a consumer
 * for UNIT_B data, ready to receive and process RESPONSE messages.
 * 
 * @param can CAN bus for communication (must not be null)
 * @param addr Network address for the agent
 * @param name Agent name for identification (default: "BasicConsumerB")
 * @return Unique pointer to configured Agent ready for operation
 * @throws std::invalid_argument if can is null or name is empty
 */
inline std::unique_ptr<Agent> create_basic_consumer_b(
    CAN* can, 
    const Agent::Address& addr, 
    const std::string& name = "BasicConsumerB"
) {
    // Parameter validation following EPOS principles
    if (!can) {
        throw std::invalid_argument("CAN bus cannot be null");
    }

    // Use default name if an empty string is provided
    const std::string& agentName = name.empty() ? "BasicConsumerB" : name;
    
    // Create component data for consumer
    auto data = std::make_unique<UnitBData>();
    
    // Create Agent using function-based composition
    return std::make_unique<Agent>(
        can,                                                    // CAN bus
        agentName,                                              // Agent name
        static_cast<std::uint32_t>(DataTypes::UNIT_B),        // Data unit
        Agent::Type::RESPONSE,                                 // Consumer observes RESPONSE
        addr,                                                  // Network address
        nullptr,                                               // No producer function for consumers
        basic_consumer_b,                                      // Response handler function pointer
        std::move(data)                                        // Component data
    );
}

/**
 * @brief Convenience function to create BasicConsumerB and start consuming
 * 
 * Creates a BasicConsumerB agent and immediately starts periodic interest
 * for the specified period. Equivalent to the original start_consuming() method.
 * 
 * @param can CAN bus for communication (must not be null)
 * @param addr Network address for the agent
 * @param period Desired response period from producers (default: 1 second)
 * @param name Agent name for identification (default: "BasicConsumerB")
 * @return Unique pointer to configured Agent with periodic interest started
 * @throws std::invalid_argument if can is null or name is empty
 */
inline std::unique_ptr<Agent> create_basic_consumer_b_with_period(
    CAN* can, 
    const Agent::Address& addr, 
    Agent::Microseconds period = Agent::Microseconds(1000000),
    const std::string& name = "BasicConsumerB"
) {
    auto consumer = create_basic_consumer_b(can, addr, name);
    consumer->start_periodic_interest(static_cast<std::uint32_t>(DataTypes::UNIT_B), period);
    return consumer;
}

#endif // BASIC_CONSUMER_B_FACTORY_HPP 