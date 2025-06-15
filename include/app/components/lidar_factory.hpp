#ifndef LIDAR_FACTORY_HPP
#define LIDAR_FACTORY_HPP

#include <memory>
#include <stdexcept>
#include "../../api/framework/agent.h"
#include "lidar_data.hpp"
#include "lidar_functions.hpp"
#include "../datatypes.h"

/**
 * @brief Creates a Lidar component using function-based composition
 * 
 * Replaces the inheritance-based LidarComponent class with EPOS SmartData
 * principles. Eliminates vtable race conditions during destruction by using
 * function pointers instead of virtual methods.
 * 
 * Lidar components are producer-only and generate 3D point cloud data with
 * variable number of points, each containing X, Y, Z coordinates and intensity.
 * This factory creates a complete Agent configured as a producer for EXTERNAL_POINT_CLOUD_XYZ.
 * 
 * @param can CAN bus for communication (must not be null)
 * @param addr Network address for the agent
 * @param name Agent name for identification (default: "LidarComponent")
 * @return Unique pointer to configured Agent ready for operation
 * @throws std::invalid_argument if can is null or name is empty
 */
inline std::unique_ptr<Agent> create_lidar_component(
    CAN* can, 
    const Agent::Address& addr, 
    const std::string& name = "LidarComponent"
) {
    // Parameter validation following EPOS principles
    if (!can) {
        throw std::invalid_argument("CAN bus cannot be null");
    }
    if (name.empty()) {
        throw std::invalid_argument("Agent name cannot be empty");
    }
    
    // Create component data for Lidar producer
    auto data = std::make_unique<LidarData>();
    
    // Create Agent using function-based composition
    // Lidar is configured as producer for EXTERNAL_POINT_CLOUD_XYZ (matching original)
    return std::make_unique<Agent>(
        can,                                                           // CAN bus
        name,                                                          // Agent name
        static_cast<std::uint32_t>(DataTypes::EXTERNAL_POINT_CLOUD_XYZ), // Data unit (matching original)
        Agent::Type::INTEREST,                                         // Producer observes INTEREST
        addr,                                                          // Network address
        lidar_producer,                                                // Data generation function pointer
        lidar_consumer,                                                // Response handler (unused for producer)
        std::move(data)                                                // Component data
    );
}

/**
 * @brief Creates a Lidar component with custom spatial ranges
 * 
 * Creates a Lidar component with customized spatial ranges for point cloud generation.
 * Useful for testing specific scenarios or configuring Lidar for different
 * operational environments (e.g., indoor vs outdoor, different vehicle types).
 * 
 * @param can CAN bus for communication (must not be null)
 * @param addr Network address for the agent
 * @param x_min Minimum X coordinate (meters)
 * @param x_max Maximum X coordinate (meters)
 * @param y_min Minimum Y coordinate (meters)
 * @param y_max Maximum Y coordinate (meters)
 * @param z_min Minimum Z coordinate (meters)
 * @param z_max Maximum Z coordinate (meters)
 * @param name Agent name for identification (default: "LidarComponent")
 * @return Unique pointer to configured Agent with custom spatial ranges
 * @throws std::invalid_argument if can is null, name is empty, or ranges are invalid
 */
inline std::unique_ptr<Agent> create_lidar_component_with_ranges(
    CAN* can, 
    const Agent::Address& addr,
    double x_min, double x_max,
    double y_min, double y_max,
    double z_min, double z_max,
    const std::string& name = "LidarComponent"
) {
    // Parameter validation
    if (!can) {
        throw std::invalid_argument("CAN bus cannot be null");
    }
    if (name.empty()) {
        throw std::invalid_argument("Agent name cannot be empty");
    }
    if (x_min >= x_max || y_min >= y_max || z_min >= z_max) {
        throw std::invalid_argument("Invalid range parameters: min must be less than max");
    }
    
    // Create component data with custom spatial ranges
    auto data = std::make_unique<LidarData>();
    data->update_spatial_range(x_min, x_max, y_min, y_max, z_min, z_max);
    
    // Create Agent using function-based composition
    return std::make_unique<Agent>(
        can,                                                           // CAN bus
        name,                                                          // Agent name
        static_cast<std::uint32_t>(DataTypes::EXTERNAL_POINT_CLOUD_XYZ), // Data unit
        Agent::Type::INTEREST,                                         // Producer observes INTEREST
        addr,                                                          // Network address
        lidar_producer,                                                // Data generation function pointer
        lidar_consumer,                                                // Response handler (unused)
        std::move(data)                                                // Component data with custom ranges
    );
}

/**
 * @brief Creates a Lidar component with custom density parameters
 * 
 * Creates a Lidar component with customized point cloud density.
 * Useful for simulating different Lidar types (low-res vs high-res) or
 * performance testing with different data volumes.
 * 
 * @param can CAN bus for communication (must not be null)
 * @param addr Network address for the agent
 * @param min_points Minimum number of points per scan
 * @param max_points Maximum number of points per scan
 * @param name Agent name for identification (default: "LidarComponent")
 * @return Unique pointer to configured Agent with custom density
 * @throws std::invalid_argument if can is null, name is empty, or parameters are invalid
 */
inline std::unique_ptr<Agent> create_lidar_component_with_density(
    CAN* can, 
    const Agent::Address& addr,
    int min_points, int max_points,
    const std::string& name = "LidarComponent"
) {
    // Parameter validation
    if (!can) {
        throw std::invalid_argument("CAN bus cannot be null");
    }
    if (name.empty()) {
        throw std::invalid_argument("Agent name cannot be empty");
    }
    if (min_points <= 0 || max_points <= min_points) {
        throw std::invalid_argument("Invalid density parameters: must have min_points > 0 and max_points > min_points");
    }
    
    // Create component data with custom density parameters
    auto data = std::make_unique<LidarData>();
    data->update_density_range(min_points, max_points);
    
    // Create Agent using function-based composition
    return std::make_unique<Agent>(
        can,                                                           // CAN bus
        name,                                                          // Agent name
        static_cast<std::uint32_t>(DataTypes::EXTERNAL_POINT_CLOUD_XYZ), // Data unit
        Agent::Type::INTEREST,                                         // Producer observes INTEREST
        addr,                                                          // Network address
        lidar_producer,                                                // Data generation function pointer
        lidar_consumer,                                                // Response handler (unused)
        std::move(data)                                                // Component data with custom density
    );
}

/**
 * @brief Creates a Lidar component with custom timing parameters
 * 
 * Creates a Lidar component with customized scan timing.
 * Useful for simulating different Lidar scan rates or testing
 * system performance under different timing conditions.
 * 
 * @param can CAN bus for communication (must not be null)
 * @param addr Network address for the agent
 * @param min_delay_ms Minimum delay between scans (milliseconds)
 * @param max_delay_ms Maximum delay between scans (milliseconds)
 * @param name Agent name for identification (default: "LidarComponent")
 * @return Unique pointer to configured Agent with custom timing
 * @throws std::invalid_argument if can is null, name is empty, or timing is invalid
 */
inline std::unique_ptr<Agent> create_lidar_component_with_timing(
    CAN* can, 
    const Agent::Address& addr,
    int min_delay_ms, int max_delay_ms,
    const std::string& name = "LidarComponent"
) {
    // Parameter validation
    if (!can) {
        throw std::invalid_argument("CAN bus cannot be null");
    }
    if (name.empty()) {
        throw std::invalid_argument("Agent name cannot be empty");
    }
    if (min_delay_ms <= 0 || max_delay_ms <= min_delay_ms) {
        throw std::invalid_argument("Invalid timing parameters: must have min_delay_ms > 0 and max_delay_ms > min_delay_ms");
    }
    
    // Create component data with custom timing parameters
    auto data = std::make_unique<LidarData>();
    data->update_timing_range(min_delay_ms, max_delay_ms);
    
    // Create Agent using function-based composition
    return std::make_unique<Agent>(
        can,                                                           // CAN bus
        name,                                                          // Agent name
        static_cast<std::uint32_t>(DataTypes::EXTERNAL_POINT_CLOUD_XYZ), // Data unit
        Agent::Type::INTEREST,                                         // Producer observes INTEREST
        addr,                                                          // Network address
        lidar_producer,                                                // Data generation function pointer
        lidar_consumer,                                                // Response handler (unused)
        std::move(data)                                                // Component data with custom timing
    );
}

#endif // LIDAR_FACTORY_HPP 