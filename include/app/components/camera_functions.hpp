#ifndef CAMERA_FUNCTIONS_HPP
#define CAMERA_FUNCTIONS_HPP

#include <vector>
#include <cstdint>
#include <cstring>
#include <thread>
#include <chrono>
#include <algorithm>
#include "camera_data.hpp"
#include "../../api/framework/component_functions.hpp"
#include "../../api/util/debug.h"

/**
 * @brief Producer function for Camera component - generates pixel matrix data
 * 
 * Replaces CameraComponent::get() virtual method with direct function call.
 * Initially simplified to generate EXTERNAL_PIXEL_MATRIX data only.
 * Generates realistic pixel matrix data with configurable dimensions and
 * noise patterns. Can be extended to support RGB_IMAGE, VIDEO_STREAM, and
 * CAMERA_METADATA in future iterations.
 * 
 * @param unit The data unit being requested (should be EXTERNAL_PIXEL_MATRIX)
 * @param data Pointer to CameraData structure containing random generators
 * @return Vector containing pixel matrix data (variable size based on image dimensions)
 */
inline std::vector<std::uint8_t> camera_producer(std::uint32_t unit, ComponentData* data) {
    CameraData* camera_data = static_cast<CameraData*>(data);
    
    if (!data) {
        db<void>(WRN) << "[CameraComponent] Received null data pointer\n";
        return std::vector<std::uint8_t>();
    }
    
    // Static message counter (function-local, maintains state between calls)
    static int message_counter = 0;
    message_counter++;
    
    // Get image dimensions and calculate total size
    std::size_t image_size = camera_data->get_image_size();
    
    // Create pixel matrix data array
    std::vector<std::uint8_t> pixel_matrix(image_size);
    
    // Generate pixel matrix data (same logic as original CameraComponent)
    // Create a simple pattern with noise for realistic image simulation
    int frame_variation = camera_data->frame_dist(camera_data->gen);
    double pattern_factor = camera_data->pattern_dist(camera_data->gen);
    
    for (int y = 0; y < camera_data->image_height; ++y) {
        for (int x = 0; x < camera_data->image_width; ++x) {
            for (int c = 0; c < camera_data->bytes_per_pixel; ++c) {
                // Generate base pixel value with pattern
                // Create a simple gradient pattern with some variation
                double normalized_x = static_cast<double>(x) / camera_data->image_width;
                double normalized_y = static_cast<double>(y) / camera_data->image_height;
                
                // Simple pattern: diagonal gradient with frame variation
                int base_value = static_cast<int>(
                    (normalized_x + normalized_y) * 127.5 * pattern_factor +
                    frame_variation * 0.5
                );
                
                // Add noise
                int noise = camera_data->noise_dist(camera_data->gen);
                int pixel_value = base_value + noise;
                
                // Clamp to valid range (0-255)
                pixel_value = std::max(0, std::min(255, pixel_value));
                
                // Store pixel value
                std::size_t pixel_index = (y * camera_data->image_width + x) * camera_data->bytes_per_pixel + c;
                pixel_matrix[pixel_index] = static_cast<std::uint8_t>(pixel_value);
            }
        }
    }
    
    // Simulate processing delay (same as original CameraComponent)
    int delay_ms = camera_data->delay_dist(camera_data->gen);
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    
    // Log pixel matrix generation (same format as original)
    db<void>(INF) << "[CameraComponent] Generated pixel matrix #" << message_counter
                  << " (" << camera_data->image_width << "x" << camera_data->image_height 
                  << "x" << camera_data->bytes_per_pixel << ")"
                  << " size: " << image_size << " bytes"
                  << " pattern: " << pattern_factor
                  << " frame_var: " << frame_variation
                  << " (delay: " << delay_ms << "ms)\n";
    
    // Additional detailed logging for debugging (sample pixel values)
    if (image_size >= 4) {
        db<void>(TRC) << "[CameraComponent] Sample pixels: "
                      << "(" << static_cast<int>(pixel_matrix[0]) << ", "
                      << static_cast<int>(pixel_matrix[1]) << ", "
                      << static_cast<int>(pixel_matrix[2]) << ", "
                      << static_cast<int>(pixel_matrix[3]) << ")\n";
    }
    
    return pixel_matrix;
}

/**
 * @brief Consumer function for Camera component (not used - Camera is producer-only)
 * 
 * Camera components are typically producer-only and don't consume data from other
 * components. This function is provided for interface compatibility but
 * performs no operations.
 * 
 * @param msg Pointer to the received message (unused)
 * @param data Pointer to CameraData structure (unused)
 */
inline void camera_consumer(void* msg, ComponentData* data) {
    // Camera is producer-only - no response handling needed
    // This function exists for interface compatibility
    db<void>(TRC) << "[CameraComponent] Consumer function called (Camera is producer-only)\n";
}

#endif // CAMERA_FUNCTIONS_HPP 