#ifndef INS_FUNCTIONS_HPP
#define INS_FUNCTIONS_HPP

#include <vector>
#include <cstdint>
#include <cstring>
#include <thread>
#include <chrono>
#include "ins_data.hpp"
#include "../../api/framework/component_functions.hpp"
#include "../../api/util/debug.h"

// INS-specific constants (local to function file scope)
namespace {
    constexpr double PI_INS = 3.14159265358979323846;
    constexpr double G_TO_MS2_INS = 9.80665;
    constexpr double DEG_TO_RAD_INS = PI_INS / 180.0;
}

/**
 * @brief Producer function for INS component - generates navigation data
 * 
 * Replaces INSComponent::get() virtual method with direct function call.
 * Generates realistic inertial navigation data including position, velocity,
 * acceleration, gyroscope readings, and heading information. Maintains the
 * exact behavior and data format of the original implementation.
 * 
 * @param unit The data unit being requested (should be EXTERNAL_INERTIAL_POSITION)
 * @param data Pointer to INSData structure containing random generators
 * @return Vector containing 32 bytes of navigation data (8 floats)
 */
inline std::vector<std::uint8_t> ins_producer(std::uint32_t unit, ComponentData* data) {
    INSData* ins_data = static_cast<INSData*>(data);
    
    if (!data) {
        db<void>(WRN) << "[INSComponent] Received null data pointer\n";
        return std::vector<std::uint8_t>();
    }
    
    // Static message counter (function-local, maintains state between calls)
    static int message_counter = 0;
    message_counter++;
    
    // Generate navigation data using the same logic as INSComponent::get()
    float x_position = static_cast<float>(ins_data->x_dist(ins_data->gen));
    float y_position = static_cast<float>(ins_data->y_dist(ins_data->gen));
    float altitude = static_cast<float>(ins_data->alt_dist(ins_data->gen));
    float velocity = static_cast<float>(ins_data->vel_dist(ins_data->gen));
    float acceleration = static_cast<float>(ins_data->accel_dist(ins_data->gen));
    float gyro_x = static_cast<float>(ins_data->gyro_dist(ins_data->gen));
    float gyro_y = static_cast<float>(ins_data->gyro_dist(ins_data->gen));
    float heading = static_cast<float>(ins_data->heading_dist(ins_data->gen));
    
    // Create data array (8 floats = 32 bytes, same as original)
    std::vector<std::uint8_t> navigation_data(8 * sizeof(float));
    
    // Pack navigation data into byte array (same order as original)
    std::memcpy(navigation_data.data() + 0 * sizeof(float), &x_position, sizeof(float));
    std::memcpy(navigation_data.data() + 1 * sizeof(float), &y_position, sizeof(float));
    std::memcpy(navigation_data.data() + 2 * sizeof(float), &altitude, sizeof(float));
    std::memcpy(navigation_data.data() + 3 * sizeof(float), &velocity, sizeof(float));
    std::memcpy(navigation_data.data() + 4 * sizeof(float), &acceleration, sizeof(float));
    std::memcpy(navigation_data.data() + 5 * sizeof(float), &gyro_x, sizeof(float));
    std::memcpy(navigation_data.data() + 6 * sizeof(float), &gyro_y, sizeof(float));
    std::memcpy(navigation_data.data() + 7 * sizeof(float), &heading, sizeof(float));
    
    // Simulate processing delay (same as original INSComponent)
    int delay_ms = ins_data->delay_dist(ins_data->gen);
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    
    // Log navigation data generation (same format as original)
    db<void>(INF) << "[INSComponent] Generated navigation data #" << message_counter
                  << " - Position: (" << x_position << ", " << y_position << ", " << altitude << ")"
                  << " Velocity: " << velocity << " m/s"
                  << " Acceleration: " << acceleration << " m/s²"
                  << " Gyro: (" << gyro_x << ", " << gyro_y << ") rad/s"
                  << " Heading: " << heading << " rad"
                  << " (delay: " << delay_ms << "ms)\n";
    
    return navigation_data;
}

/**
 * @brief Consumer function for INS component (not used - INS is producer-only)
 * 
 * INS components are typically producer-only and don't consume data from other
 * components. This function is provided for interface compatibility but
 * performs no operations.
 * 
 * @param msg Pointer to the received message (unused)
 * @param data Pointer to INSData structure (unused)
 */
inline void ins_consumer(void* msg, ComponentData* data) {
    // INS is producer-only - no response handling needed
    // This function exists for interface compatibility
    db<void>(TRC) << "[INSComponent] Consumer function called (INS is producer-only)\n";
}

#endif // INS_FUNCTIONS_HPP 