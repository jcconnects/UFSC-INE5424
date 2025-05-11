#ifndef TEDS_H
#define TEDS_H

#include <cstdint>

enum class DataTypeId : std::uint32_t {
    UNKNOWN = 0,
    VEHICLE_SPEED,
    ENGINE_RPM,
    OBSTACLE_DISTANCE,
    // Add other domain-specific data types here
    SYSTEM_INTERNAL_REG_PRODUCER, // For Gateway registration
    // Example additional types
    TEMPERATURE_SENSOR,
    GPS_POSITION,
    CUSTOM_SENSOR_DATA_A,
    CUSTOM_SENSOR_DATA_B
};

// Add comparison operator to allow comparing DataTypeId with integer literals
inline bool operator==(const DataTypeId& lhs, const int rhs) {
    return static_cast<std::uint32_t>(lhs) == static_cast<std::uint32_t>(rhs);
}

// Add reverse comparison operator for symmetry
inline bool operator==(const int lhs, const DataTypeId& rhs) {
    return rhs == lhs;
}

#endif // TEDS_H 