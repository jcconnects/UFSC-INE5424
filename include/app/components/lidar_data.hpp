#ifndef LIDAR_DATA_HPP
#define LIDAR_DATA_HPP

#include <random>
#include "../../api/framework/component_types.hpp"

/**
 * @brief Data structure for Lidar component
 * 
 * Following EPOS SmartData principles, this structure contains all the data
 * needed for LidarComponent functionality. Generates realistic 3D point cloud
 * data with configurable ranges and densities for simulation purposes.
 * 
 * Replaces inheritance-based data storage with pure composition.
 */
struct LidarData : public ComponentData {
    // Producer data: random number generation for point cloud data
    std::random_device rd;
    std::mt19937 gen;
    
    // Point cloud generation distributions
    std::uniform_real_distribution<> x_dist;        // X coordinate (meters)
    std::uniform_real_distribution<> y_dist;        // Y coordinate (meters)
    std::uniform_real_distribution<> z_dist;        // Z coordinate (meters)
    std::uniform_real_distribution<> intensity_dist; // Point intensity (0.0-1.0)
    
    // Point cloud size distribution
    std::uniform_int_distribution<> num_points_dist; // Number of points per scan
    
    // Timing distribution
    std::uniform_int_distribution<> delay_dist;     // Delay between scans (ms)
    
    /**
     * @brief Constructor initializes random number generation with realistic ranges
     * 
     * Sets up the same random distributions as the original LidarComponent:
     * - X/Y coordinates: ±50m (typical lidar range for automotive)
     * - Z coordinate: -5 to +10m (ground to overhead obstacles)
     * - Intensity: 0.0-1.0 (normalized reflectance values)
     * - Points per scan: 1000-5000 (typical automotive lidar density)
     * - Delay: 90-110ms (typical lidar ~10Hz scan rate)
     */
    LidarData() : gen(rd()),
                  x_dist(-50.0, 50.0),              // X coordinate in meters (±50m)
                  y_dist(-50.0, 50.0),              // Y coordinate in meters (±50m)
                  z_dist(-5.0, 10.0),               // Z coordinate in meters (-5m to +10m)
                  intensity_dist(0.0, 1.0),         // Intensity (normalized 0.0-1.0)
                  num_points_dist(1000, 5000),      // Number of points per scan
                  delay_dist(90, 110) {}            // Milliseconds delay (lidar typically ~10Hz)
    
    /**
     * @brief Update the point cloud generation ranges
     * 
     * Allows dynamic reconfiguration of the point cloud spatial ranges.
     * Useful for testing different scenarios or runtime configuration.
     * 
     * @param x_min Minimum X coordinate
     * @param x_max Maximum X coordinate
     * @param y_min Minimum Y coordinate
     * @param y_max Maximum Y coordinate
     * @param z_min Minimum Z coordinate
     * @param z_max Maximum Z coordinate
     */
    void update_spatial_range(double x_min, double x_max, double y_min, double y_max, 
                             double z_min, double z_max) {
        x_dist = std::uniform_real_distribution<>(x_min, x_max);
        y_dist = std::uniform_real_distribution<>(y_min, y_max);
        z_dist = std::uniform_real_distribution<>(z_min, z_max);
    }
    
    /**
     * @brief Update the point cloud density parameters
     * 
     * @param min_points Minimum number of points per scan
     * @param max_points Maximum number of points per scan
     */
    void update_density_range(int min_points, int max_points) {
        if (min_points > 0 && max_points > min_points) {
            num_points_dist = std::uniform_int_distribution<>(min_points, max_points);
        }
    }
    
    /**
     * @brief Update the scan timing parameters
     * 
     * @param min_delay_ms Minimum delay between scans (milliseconds)
     * @param max_delay_ms Maximum delay between scans (milliseconds)
     */
    void update_timing_range(int min_delay_ms, int max_delay_ms) {
        if (min_delay_ms > 0 && max_delay_ms > min_delay_ms) {
            delay_dist = std::uniform_int_distribution<>(min_delay_ms, max_delay_ms);
        }
    }
};

#endif // LIDAR_DATA_HPP 