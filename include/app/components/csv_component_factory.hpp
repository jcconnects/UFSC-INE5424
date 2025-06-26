#ifndef CSV_COMPONENT_FACTORY_HPP
#define CSV_COMPONENT_FACTORY_HPP

#include <memory>
#include <stdexcept>
#include "../../api/framework/agent.h"
#include "csv_component_data.hpp"
#include "csv_component_functions.hpp"
#include "../datatypes.h"

/**
 * @brief Creates a CSV component using function-based composition
 * 
 * Replaces inheritance-based CSV component with EPOS SmartData
 * principles. Eliminates vtable race conditions during destruction by using
 * function pointers instead of virtual methods.
 * 
 * CSV components are producer-only and read data from CSV files in the format:
 * timestamp,id,lat,lon,alt,x,y,z,speed,heading,yawrate,acceleration
 * This factory creates a complete Agent configured as a producer for CSV_VEHICLE_DATA.
 * 
 * @param can CAN bus for communication (must not be null)
 * @param addr Network address for the agent
 * @param csv_file_path Path to the CSV file to read
 * @param name Agent name for identification (default: "CSVComponent")
 * @return Unique pointer to configured Agent ready for operation
 * @throws std::invalid_argument if can is null, csv_file_path is empty, or file cannot be loaded
 */
inline std::unique_ptr<Agent> create_csv_component(
    CAN* can, 
    const Agent::Address& addr, 
    const std::string& csv_file_path,
    const std::string& name = "CSVComponent"
) {
    // Parameter validation following EPOS principles
    if (!can) {
        throw std::invalid_argument("CAN bus cannot be null");
    }

    if (csv_file_path.empty()) {
        throw std::invalid_argument("CSV file path cannot be empty");
    }

    // Use default name if an empty string is provided
    const std::string& agentName = name.empty() ? "CSVComponent" : name;

    // Create component data for CSV producer
    auto data = std::make_unique<CSVComponentData>();
    
    // Load CSV file
    if (!data->load_csv_file(csv_file_path)) {
        throw std::invalid_argument("Failed to load CSV file: " + csv_file_path);
    }
    
    // Create Agent using function-based composition
    // CSV is configured as producer for CSV_VEHICLE_DATA (we'll need to add this to datatypes)
    return std::make_unique<Agent>(
        can,                                                           // CAN bus
        agentName,                                                     // Agent name
        static_cast<std::uint32_t>(DataTypes::EXTERNAL_CSV_VEHICLE_DATA), // CSV vehicle data type
        Agent::Type::INTEREST,                                         // Producer observes INTEREST
        addr,                                                          // Network address
        csv_producer,                                                  // Data generation function pointer
        csv_consumer,                                                  // Response handler (empty implementation)
        std::move(data)                                                // Component data
    );
}

/**
 * @brief Creates a CSV component with a specific dynamics-vehicle CSV file
 * 
 * Creates a CSV component specifically configured for dynamics-vehicle CSV files.
 * This is a convenience function for the common use case of loading vehicle dynamics data.
 * 
 * @param can CAN bus for communication (must not be null)
 * @param addr Network address for the agent
 * @param vehicle_id Vehicle ID (0-14 based on available files)
 * @param name Agent name for identification (default: "DynamicsCSVComponent")
 * @return Unique pointer to configured Agent with dynamics vehicle data
 * @throws std::invalid_argument if can is null, vehicle_id is invalid, or file cannot be loaded
 */
inline std::unique_ptr<Agent> create_dynamics_csv_component(
    CAN* can, 
    const Agent::Address& addr,
    int vehicle_id,
    const std::string& name = "DynamicsCSVComponent"
) {
    // Parameter validation
    if (!can) {
        throw std::invalid_argument("CAN bus cannot be null");
    }

    if (vehicle_id < 0 || vehicle_id > 14) {
        throw std::invalid_argument("Invalid vehicle_id: must be between 0 and 14");
    }

    // Use default name if an empty string is provided
    const std::string& agentName = name.empty() ? "DynamicsCSVComponent" : name;

    // Construct path to dynamics vehicle CSV file
    std::string csv_file_path = "include/app/components/datasets/dataset/dynamics-vehicle_" + 
                               std::to_string(vehicle_id) + ".csv";
    
    // Create component data
    auto data = std::make_unique<CSVComponentData>();
    
    // Load CSV file
    if (!data->load_csv_file(csv_file_path)) {
        throw std::invalid_argument("Failed to load dynamics vehicle CSV file for vehicle " + 
                                   std::to_string(vehicle_id));
    }
    
    // Create Agent using function-based composition
    return std::make_unique<Agent>(
        can,                                                           // CAN bus
        agentName,                                                     // Agent name
        static_cast<std::uint32_t>(DataTypes::EXTERNAL_SENSOR_DATA),   // Using existing data type
        Agent::Type::INTEREST,                                         // Producer observes INTEREST
        addr,                                                          // Network address
        csv_producer,                                                  // Data generation function pointer
        csv_consumer,                                                  // Response handler (empty)
        std::move(data)                                                // Component data with loaded CSV
    );
}

/**
 * @brief Creates a CSV component with a specific perception-vehicle CSV file
 * 
 * Creates a CSV component specifically configured for perception-vehicle CSV files.
 * This is a convenience function for the common use case of loading perception data.
 * 
 * @param can CAN bus for communication (must not be null)
 * @param addr Network address for the agent
 * @param vehicle_id Vehicle ID (0-14 based on available files)
 * @param name Agent name for identification (default: "PerceptionCSVComponent")
 * @return Unique pointer to configured Agent with perception vehicle data
 * @throws std::invalid_argument if can is null, vehicle_id is invalid, or file cannot be loaded
 */
inline std::unique_ptr<Agent> create_perception_csv_component(
    CAN* can, 
    const Agent::Address& addr,
    int vehicle_id,
    const std::string& name = "PerceptionCSVComponent"
) {
    // Parameter validation
    if (!can) {
        throw std::invalid_argument("CAN bus cannot be null");
    }

    if (vehicle_id < 0 || vehicle_id > 14) {
        throw std::invalid_argument("Invalid vehicle_id: must be between 0 and 14");
    }

    // Use default name if an empty string is provided
    const std::string& agentName = name.empty() ? "PerceptionCSVComponent" : name;

    // Construct path to perception vehicle CSV file
    std::string csv_file_path = "include/app/components/datasets/dataset/perception-vehicle_" + 
                               std::to_string(vehicle_id) + ".csv";
    
    // Create component data
    auto data = std::make_unique<CSVComponentData>();
    
    // Load CSV file
    if (!data->load_csv_file(csv_file_path)) {
        throw std::invalid_argument("Failed to load perception vehicle CSV file for vehicle " + 
                                   std::to_string(vehicle_id));
    }
    
    // Create Agent using function-based composition
    return std::make_unique<Agent>(
        can,                                                           // CAN bus
        agentName,                                                     // Agent name
        static_cast<std::uint32_t>(DataTypes::EXTERNAL_SENSOR_DATA),   // Using existing data type
        Agent::Type::INTEREST,                                         // Producer observes INTEREST
        addr,                                                          // Network address
        csv_producer,                                                  // Data generation function pointer
        csv_consumer,                                                  // Response handler (empty)
        std::move(data)                                                // Component data with loaded CSV
    );
}

#endif // CSV_COMPONENT_FACTORY_HPP 