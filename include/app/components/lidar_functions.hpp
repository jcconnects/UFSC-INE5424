#ifndef LIDAR_FUNCTIONS_HPP
#define LIDAR_FUNCTIONS_HPP

#include <vector>
#include <cstdint>
#include <cstring>
#include <thread>
#include <chrono>
#include "lidar_data.hpp"
#include "../../api/framework/component_functions.hpp"
#include "../../api/util/debug.h"

/**
 * @brief Producer function for Lidar component - generates point cloud data
 * 
 * Replaces LidarComponent::get() virtual method with direct function call.
 * Generates realistic 3D point cloud data with variable number of points,
 * each containing X, Y, Z coordinates and intensity values. Maintains the
 * exact behavior and data format of the original implementation.
 * 
 * @param unit The data unit being requested (should be EXTERNAL_POINT_CLOUD_XYZ)
 * @param data Pointer to LidarData structure containing random generators
 * @return Vector containing point cloud data (variable size based on number of points)
 */
inline std::vector<std::uint8_t> lidar_producer(std::uint32_t unit, ComponentData* data) {
    LidarData* lidar_data = static_cast<LidarData*>(data);
    
    if (!data) {
        db<void>(WRN) << "[LidarComponent] Received null data pointer\n";
        return std::vector<std::uint8_t>();
    }
    
    // Static message counter (function-local, maintains state between calls)
    static int message_counter = 0;
    message_counter++;
    
    // Generate number of points for this scan (same logic as LidarComponent::get())
    int num_points = lidar_data->num_points_dist(lidar_data->gen);
    
    // Create point cloud data array
    // Each point: 4 floats (X, Y, Z, Intensity) = 16 bytes per point
    std::size_t data_size = num_points * 4 * sizeof(float);
    std::vector<std::uint8_t> point_cloud_data(data_size);
    
    // Generate point cloud data (same logic as original LidarComponent)
    for (int i = 0; i < num_points; ++i) {
        // Generate point coordinates and intensity
        float x = static_cast<float>(lidar_data->x_dist(lidar_data->gen));
        float y = static_cast<float>(lidar_data->y_dist(lidar_data->gen));
        float z = static_cast<float>(lidar_data->z_dist(lidar_data->gen));
        float intensity = static_cast<float>(lidar_data->intensity_dist(lidar_data->gen));
        
        // Pack point data into byte array (same order as original)
        std::size_t point_offset = i * 4 * sizeof(float);
        std::memcpy(point_cloud_data.data() + point_offset + 0 * sizeof(float), &x, sizeof(float));
        std::memcpy(point_cloud_data.data() + point_offset + 1 * sizeof(float), &y, sizeof(float));
        std::memcpy(point_cloud_data.data() + point_offset + 2 * sizeof(float), &z, sizeof(float));
        std::memcpy(point_cloud_data.data() + point_offset + 3 * sizeof(float), &intensity, sizeof(float));
    }
    
    // Simulate processing delay (same as original LidarComponent)
    int delay_ms = lidar_data->delay_dist(lidar_data->gen);
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    
    // Log point cloud generation (same format as original)
    db<void>(INF) << "[LidarComponent] Generated point cloud #" << message_counter
                  << " with " << num_points << " points"
                  << " (" << data_size << " bytes)"
                  << " (delay: " << delay_ms << "ms)\n";
    
    // Additional detailed logging for first few points (debugging)
    if (num_points > 0) {
        float first_x, first_y, first_z, first_intensity;
        std::memcpy(&first_x, point_cloud_data.data() + 0 * sizeof(float), sizeof(float));
        std::memcpy(&first_y, point_cloud_data.data() + 1 * sizeof(float), sizeof(float));
        std::memcpy(&first_z, point_cloud_data.data() + 2 * sizeof(float), sizeof(float));
        std::memcpy(&first_intensity, point_cloud_data.data() + 3 * sizeof(float), sizeof(float));
        
        db<void>(TRC) << "[LidarComponent] First point: (" << first_x << ", " << first_y 
                      << ", " << first_z << ") intensity: " << first_intensity << "\n";
    }
    
    return point_cloud_data;
}

/**
 * @brief Consumer function for Lidar component (not used - Lidar is producer-only)
 * 
 * Lidar components are typically producer-only and don't consume data from other
 * components. This function is provided for interface compatibility but
 * performs no operations.
 * 
 * @param msg Pointer to the received message (unused)
 * @param data Pointer to LidarData structure (unused)
 */
inline void lidar_consumer(void* msg, ComponentData* data) {
    // Lidar is producer-only - no response handling needed
    // This function exists for interface compatibility
    db<void>(TRC) << "[LidarComponent] Consumer function called (Lidar is producer-only)\n";
}

#endif // LIDAR_FUNCTIONS_HPP 