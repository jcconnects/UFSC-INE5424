#ifndef INS_DATA_HPP
#define INS_DATA_HPP

#include <random>
#include "../../api/framework/component_types.hpp"

/**
 * @brief Data structure for INS (Inertial Navigation System) component
 * 
 * Following EPOS SmartData principles, this structure contains all the data
 * needed for INSComponent functionality. Generates realistic navigation data
 * including position, velocity, acceleration, gyroscope, and heading information.
 * 
 * Replaces inheritance-based data storage with pure composition.
 */
struct INSData : public ComponentData {
    // Producer data: random number generation for navigation data
    std::random_device rd;
    std::mt19937 gen;
    
    // Position distributions
    std::uniform_real_distribution<> x_dist;      // X coordinate (meters)
    std::uniform_real_distribution<> y_dist;      // Y coordinate (meters)
    std::uniform_real_distribution<> alt_dist;    // Altitude (meters)
    
    // Motion distributions
    std::uniform_real_distribution<> vel_dist;    // Velocity (m/s)
    std::uniform_real_distribution<> accel_dist;  // Acceleration (m/s²)
    std::uniform_real_distribution<> gyro_dist;   // Gyroscope (rad/s)
    std::uniform_real_distribution<> heading_dist; // Heading (radians)
    
    // Timing distribution
    std::uniform_int_distribution<> delay_dist;   // Delay between readings (ms)
    
    /**
     * @brief Constructor initializes random number generation with realistic ranges
     * 
     * Sets up the same random distributions as the original INSComponent:
     * - Position: 0-1000m for X/Y, 0-500m altitude
     * - Velocity: 0-30 m/s (typical vehicle speeds)
     * - Acceleration: ±2g (realistic vehicle acceleration)
     * - Gyroscope: ±π rad/s (±180 deg/s)
     * - Heading: 0-2π rad (0-360 degrees)
     * - Delay: 90-110ms (typical INS ~10Hz update rate)
     */
    INSData() : gen(rd()),
                x_dist(0.0, 1000.0),                    // X coordinate in meters (0 to 1000m)
                y_dist(0.0, 1000.0),                    // Y coordinate in meters (0 to 1000m)
                alt_dist(0.0, 500.0),                   // Altitude meters
                vel_dist(0.0, 30.0),                    // Velocity m/s
                accel_dist(-2.0 * 9.80665, 2.0 * 9.80665), // Acceleration m/s² (±2g)
                gyro_dist(-3.14159265358979323846, 3.14159265358979323846), // Gyro rad/s (±π)
                heading_dist(0, 2.0 * 3.14159265358979323846), // Heading rad (0 to 2π)
                delay_dist(90, 110) {}                  // Milliseconds delay (INS typically ~10Hz)
    
    /**
     * @brief Update the navigation data generation ranges
     * 
     * Allows dynamic reconfiguration of the navigation data ranges.
     * Useful for testing different scenarios or runtime configuration.
     * 
     * @param x_min Minimum X coordinate
     * @param x_max Maximum X coordinate
     * @param y_min Minimum Y coordinate
     * @param y_max Maximum Y coordinate
     * @param alt_min Minimum altitude
     * @param alt_max Maximum altitude
     */
    void update_position_range(double x_min, double x_max, double y_min, double y_max, 
                              double alt_min, double alt_max) {
        x_dist = std::uniform_real_distribution<>(x_min, x_max);
        y_dist = std::uniform_real_distribution<>(y_min, y_max);
        alt_dist = std::uniform_real_distribution<>(alt_min, alt_max);
    }
    
    /**
     * @brief Update the motion data generation ranges
     * 
     * @param vel_min Minimum velocity (m/s)
     * @param vel_max Maximum velocity (m/s)
     * @param accel_min Minimum acceleration (m/s²)
     * @param accel_max Maximum acceleration (m/s²)
     */
    void update_motion_range(double vel_min, double vel_max, double accel_min, double accel_max) {
        vel_dist = std::uniform_real_distribution<>(vel_min, vel_max);
        accel_dist = std::uniform_real_distribution<>(accel_min, accel_max);
    }
};

#endif // INS_DATA_HPP 