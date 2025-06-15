#ifndef CAMERA_FACTORY_HPP
#define CAMERA_FACTORY_HPP

#include <memory>
#include <stdexcept>
#include "../../api/framework/agent_v2.hpp"
#include "../../api/framework/component_functions.hpp"
#include "camera_data.hpp"
#include "camera_functions.hpp"
#include "../datatypes.h"

/**
 * @brief Creates a Camera component using function-based composition
 * 
 * Replaces the inheritance-based CameraComponent class with EPOS SmartData
 * principles. Eliminates vtable race conditions during destruction by using
 * function pointers instead of virtual methods.
 * 
 * Camera components are producer-only and generate pixel matrix data.
 * Initially simplified to handle EXTERNAL_PIXEL_MATRIX only, but can be
 * extended to support RGB_IMAGE, VIDEO_STREAM, and CAMERA_METADATA.
 * This factory creates a complete Agent configured as a producer for EXTERNAL_PIXEL_MATRIX.
 * 
 * @param can CAN bus for communication (must not be null)
 * @param addr Network address for the agent
 * @param name Agent name for identification (default: "CameraComponent")
 * @return Unique pointer to configured Agent ready for operation
 * @throws std::invalid_argument if can is null or name is empty
 */
inline std::unique_ptr<Agent> create_camera_component(
    CAN* can, 
    const Agent::Address& addr, 
    const std::string& name = "CameraComponent"
) {
    // Parameter validation following EPOS principles
    if (!can) {
        throw std::invalid_argument("CAN bus cannot be null");
    }
    if (name.empty()) {
        throw std::invalid_argument("Agent name cannot be empty");
    }
    
    // Create component data for Camera producer
    auto data = std::make_unique<CameraData>();
    
    // Create Agent using function-based composition
    // Camera is configured as producer for EXTERNAL_PIXEL_MATRIX (initially simplified)
    return std::make_unique<Agent>(
        can,                                                           // CAN bus
        name,                                                          // Agent name
        static_cast<std::uint32_t>(DataTypes::EXTERNAL_PIXEL_MATRIX),  // Data unit (simplified)
        Agent::Type::INTEREST,                                         // Producer observes INTEREST
        addr,                                                          // Network address
        camera_producer,                                               // Data generation function pointer
        camera_consumer,                                               // Response handler (unused for producer)
        std::move(data)                                                // Component data
    );
}

/**
 * @brief Creates a Camera component with custom image dimensions
 * 
 * Creates a Camera component with customized image resolution and color depth.
 * Useful for testing different camera configurations or simulating different
 * camera types (e.g., low-res vs high-res, grayscale vs color).
 * 
 * @param can CAN bus for communication (must not be null)
 * @param addr Network address for the agent
 * @param width Image width in pixels
 * @param height Image height in pixels
 * @param bytes_per_pixel Bytes per pixel (1 for grayscale, 3 for RGB)
 * @param name Agent name for identification (default: "CameraComponent")
 * @return Unique pointer to configured Agent with custom dimensions
 * @throws std::invalid_argument if can is null, name is empty, or dimensions are invalid
 */
inline std::unique_ptr<Agent> create_camera_component_with_dimensions(
    CAN* can, 
    const Agent::Address& addr,
    int width, int height, int bytes_per_pixel = 1,
    const std::string& name = "CameraComponent"
) {
    // Parameter validation
    if (!can) {
        throw std::invalid_argument("CAN bus cannot be null");
    }
    if (name.empty()) {
        throw std::invalid_argument("Agent name cannot be empty");
    }
    if (width <= 0 || height <= 0 || bytes_per_pixel <= 0) {
        throw std::invalid_argument("Invalid image dimensions: width, height, and bytes_per_pixel must be positive");
    }
    if (bytes_per_pixel > 4) {
        throw std::invalid_argument("Invalid bytes_per_pixel: maximum supported is 4 (RGBA)");
    }
    
    // Create component data with custom dimensions
    auto data = std::make_unique<CameraData>();
    data->update_image_dimensions(width, height, bytes_per_pixel);
    
    // Create Agent using function-based composition
    return std::make_unique<Agent>(
        can,                                                           // CAN bus
        name,                                                          // Agent name
        static_cast<std::uint32_t>(DataTypes::EXTERNAL_PIXEL_MATRIX),  // Data unit
        Agent::Type::INTEREST,                                         // Producer observes INTEREST
        addr,                                                          // Network address
        camera_producer,                                               // Data generation function pointer
        camera_consumer,                                               // Response handler (unused)
        std::move(data)                                                // Component data with custom dimensions
    );
}

/**
 * @brief Creates a Camera component with custom pixel parameters
 * 
 * Creates a Camera component with customized pixel value ranges and noise characteristics.
 * Useful for simulating different lighting conditions, sensor characteristics,
 * or testing system robustness under various image quality conditions.
 * 
 * @param can CAN bus for communication (must not be null)
 * @param addr Network address for the agent
 * @param min_pixel Minimum pixel value (0-255)
 * @param max_pixel Maximum pixel value (0-255)
 * @param noise_range Noise variation range (±noise_range)
 * @param name Agent name for identification (default: "CameraComponent")
 * @return Unique pointer to configured Agent with custom pixel parameters
 * @throws std::invalid_argument if can is null, name is empty, or parameters are invalid
 */
inline std::unique_ptr<Agent> create_camera_component_with_pixel_params(
    CAN* can, 
    const Agent::Address& addr,
    int min_pixel, int max_pixel, int noise_range,
    const std::string& name = "CameraComponent"
) {
    // Parameter validation
    if (!can) {
        throw std::invalid_argument("CAN bus cannot be null");
    }
    if (name.empty()) {
        throw std::invalid_argument("Agent name cannot be empty");
    }
    if (min_pixel < 0 || max_pixel > 255 || min_pixel >= max_pixel) {
        throw std::invalid_argument("Invalid pixel range: must have 0 <= min_pixel < max_pixel <= 255");
    }
    if (noise_range < 0) {
        throw std::invalid_argument("Invalid noise range: must be non-negative");
    }
    
    // Create component data with custom pixel parameters
    auto data = std::make_unique<CameraData>();
    data->update_pixel_range(min_pixel, max_pixel, noise_range);
    
    // Create Agent using function-based composition
    return std::make_unique<Agent>(
        can,                                                           // CAN bus
        name,                                                          // Agent name
        static_cast<std::uint32_t>(DataTypes::EXTERNAL_PIXEL_MATRIX),  // Data unit
        Agent::Type::INTEREST,                                         // Producer observes INTEREST
        addr,                                                          // Network address
        camera_producer,                                               // Data generation function pointer
        camera_consumer,                                               // Response handler (unused)
        std::move(data)                                                // Component data with custom pixel params
    );
}

/**
 * @brief Creates a Camera component with custom timing parameters
 * 
 * Creates a Camera component with customized frame timing.
 * Useful for simulating different camera frame rates or testing
 * system performance under different timing conditions.
 * 
 * @param can CAN bus for communication (must not be null)
 * @param addr Network address for the agent
 * @param min_delay_ms Minimum delay between frames (milliseconds)
 * @param max_delay_ms Maximum delay between frames (milliseconds)
 * @param name Agent name for identification (default: "CameraComponent")
 * @return Unique pointer to configured Agent with custom timing
 * @throws std::invalid_argument if can is null, name is empty, or timing is invalid
 */
inline std::unique_ptr<Agent> create_camera_component_with_timing(
    CAN* can, 
    const Agent::Address& addr,
    int min_delay_ms, int max_delay_ms,
    const std::string& name = "CameraComponent"
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
    auto data = std::make_unique<CameraData>();
    data->update_timing_range(min_delay_ms, max_delay_ms);
    
    // Create Agent using function-based composition
    return std::make_unique<Agent>(
        can,                                                           // CAN bus
        name,                                                          // Agent name
        static_cast<std::uint32_t>(DataTypes::EXTERNAL_PIXEL_MATRIX),  // Data unit
        Agent::Type::INTEREST,                                         // Producer observes INTEREST
        addr,                                                          // Network address
        camera_producer,                                               // Data generation function pointer
        camera_consumer,                                               // Response handler (unused)
        std::move(data)                                                // Component data with custom timing
    );
}

/**
 * @brief Creates a fully customized Camera component
 * 
 * Creates a Camera component with all parameters customizable.
 * Provides maximum flexibility for testing and configuration.
 * 
 * @param can CAN bus for communication (must not be null)
 * @param addr Network address for the agent
 * @param width Image width in pixels
 * @param height Image height in pixels
 * @param bytes_per_pixel Bytes per pixel (1 for grayscale, 3 for RGB)
 * @param min_pixel Minimum pixel value (0-255)
 * @param max_pixel Maximum pixel value (0-255)
 * @param noise_range Noise variation range (±noise_range)
 * @param min_delay_ms Minimum delay between frames (milliseconds)
 * @param max_delay_ms Maximum delay between frames (milliseconds)
 * @param name Agent name for identification (default: "CameraComponent")
 * @return Unique pointer to fully configured Agent
 * @throws std::invalid_argument if any parameters are invalid
 */
inline std::unique_ptr<Agent> create_camera_component_fully_custom(
    CAN* can, 
    const Agent::Address& addr,
    int width, int height, int bytes_per_pixel,
    int min_pixel, int max_pixel, int noise_range,
    int min_delay_ms, int max_delay_ms,
    const std::string& name = "CameraComponent"
) {
    // Parameter validation
    if (!can) {
        throw std::invalid_argument("CAN bus cannot be null");
    }
    if (name.empty()) {
        throw std::invalid_argument("Agent name cannot be empty");
    }
    if (width <= 0 || height <= 0 || bytes_per_pixel <= 0 || bytes_per_pixel > 4) {
        throw std::invalid_argument("Invalid image dimensions");
    }
    if (min_pixel < 0 || max_pixel > 255 || min_pixel >= max_pixel || noise_range < 0) {
        throw std::invalid_argument("Invalid pixel parameters");
    }
    if (min_delay_ms <= 0 || max_delay_ms <= min_delay_ms) {
        throw std::invalid_argument("Invalid timing parameters");
    }
    
    // Create component data with all custom parameters
    auto data = std::make_unique<CameraData>();
    data->update_image_dimensions(width, height, bytes_per_pixel);
    data->update_pixel_range(min_pixel, max_pixel, noise_range);
    data->update_timing_range(min_delay_ms, max_delay_ms);
    
    // Create Agent using function-based composition
    return std::make_unique<Agent>(
        can,                                                           // CAN bus
        name,                                                          // Agent name
        static_cast<std::uint32_t>(DataTypes::EXTERNAL_PIXEL_MATRIX),  // Data unit
        Agent::Type::INTEREST,                                         // Producer observes INTEREST
        addr,                                                          // Network address
        camera_producer,                                               // Data generation function pointer
        camera_consumer,                                               // Response handler (unused)
        std::move(data)                                                // Component data with all custom params
    );
}

#endif // CAMERA_FACTORY_HPP 