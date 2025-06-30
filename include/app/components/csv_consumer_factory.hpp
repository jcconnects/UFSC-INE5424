#ifndef CSV_CONSUMER_FACTORY_HPP
#define CSV_CONSUMER_FACTORY_HPP

#include <memory>
#include <stdexcept>
#include "../../api/framework/agent.h"
#include "csv_consumer_data.hpp"
#include "csv_consumer_functions.hpp"
#include "../datatypes.h"

/**
 * @brief Creates a CSV consumer component using function-based composition
 * 
 * Creates a consumer-only component that exclusively processes CSV_VEHICLE_DATA
 * messages from CSV producer components. The consumer extracts timestamps and
 * CSV record data from received messages and provides detailed logging and
 * statistics tracking.
 * 
 * This component is designed to work with CSV producers that prepend timestamps
 * to their messages as implemented in csv_producer function.
 * 
 * @param can CAN bus for communication (must not be null)
 * @param addr Network address for the agent
 * @param name Agent name for identification (default: "CSVConsumer")
 * @return Unique pointer to configured Agent ready for consuming CSV data
 * @throws std::invalid_argument if can is null
 */
inline std::unique_ptr<Agent> create_csv_consumer(
    CAN* can, 
    const Agent::Address& addr, 
    const std::string& name = "CSVConsumer"
) {
    // Parameter validation following EPOS principles
    if (!can) {
        throw std::invalid_argument("CAN bus cannot be null");
    }

    // Use default name if an empty string is provided
    const std::string& agentName = name.empty() ? "CSVConsumer" : name;

    // Create component data for CSV consumer
    auto data = std::make_unique<CSVConsumerData>();
    
    // Create Agent using function-based composition
    // CSV consumer is configured as consumer for CSV_VEHICLE_DATA
    return std::make_unique<Agent>(
        can,                                                           // CAN bus
        agentName,                                                     // Agent name
        static_cast<std::uint32_t>(DataTypes::CSV_VEHICLE_DATA),       // CSV vehicle data type
        Agent::Type::RESPONSE,                                         // Consumer handles RESPONSE messages
        addr,                                                          // Network address
        csv_consumer_producer,                                         // Empty producer function
        csv_consumer_consumer,                                         // CSV data processing function
        std::move(data)                                                // Consumer tracking data
    );
}

/**
 * @brief Creates a CSV consumer component for external CSV vehicle data
 * 
 * Creates a consumer component specifically configured for external CSV vehicle data.
 * This is a convenience function for consuming CSV data from external sources.
 * 
 * @param can CAN bus for communication (must not be null)
 * @param addr Network address for the agent
 * @param name Agent name for identification (default: "ExternalCSVConsumer")
 * @return Unique pointer to configured Agent for consuming external CSV data
 * @throws std::invalid_argument if can is null
 */
inline std::unique_ptr<Agent> create_external_csv_consumer(
    CAN* can, 
    const Agent::Address& addr,
    const std::string& name = "ExternalCSVConsumer"
) {
    // Parameter validation
    if (!can) {
        throw std::invalid_argument("CAN bus cannot be null");
    }

    // Use default name if an empty string is provided
    const std::string& agentName = name.empty() ? "ExternalCSVConsumer" : name;

    // Create component data
    auto data = std::make_unique<CSVConsumerData>();
    
    // Create Agent using function-based composition
    return std::make_unique<Agent>(
        can,                                                           // CAN bus
        agentName,                                                     // Agent name
        static_cast<std::uint32_t>(DataTypes::EXTERNAL_CSV_VEHICLE_DATA), // External CSV vehicle data
        Agent::Type::RESPONSE,                                         // Consumer handles RESPONSE messages
        addr,                                                          // Network address
        csv_consumer_producer,                                         // Empty producer function
        csv_consumer_consumer,                                         // CSV data processing function
        std::move(data)                                                // Consumer tracking data
    );
}

#endif // CSV_CONSUMER_FACTORY_HPP 