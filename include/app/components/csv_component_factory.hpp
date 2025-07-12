#ifndef CSV_COMPONENT_FACTORY_HPP
#define CSV_COMPONENT_FACTORY_HPP

#include <memory>
#include <stdexcept>
#include "../../api/framework/agent.h"
#include "../../api/framework/csv_agent.h"
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
    
    // Create CSVAgent using function-based composition
    // CSV is configured as producer for CSV_VEHICLE_DATA (we'll need to add this to datatypes)
    return std::make_unique<CSVAgent>(
        can,                                                           // CAN bus
        agentName,                                                     // Agent name
        static_cast<std::uint32_t>(DataTypes::CSV_VEHICLE_DATA),       // CSV vehicle data type
        Agent::Type::INTEREST,                                         // Producer observes INTEREST
        addr,                                                          // Network address
        csv_producer,                                                  // Data generation function pointer
        csv_consumer,                                                  // Response handler (empty implementation)
        std::move(data),                                               // Component data
        true                                                          // Not external
    );
}

#endif // CSV_COMPONENT_FACTORY_HPP 