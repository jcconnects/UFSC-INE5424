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

#endif // TEDS_H 