#ifndef DATATYPES_H
#define DATATYPES_H

#include <cstdint>

enum class DataTypes : std::uint32_t {
    /* Internal (bit 31 = 0) */
    
    // Simple test units
    UNIT_A                   = 0x00000055,
    UNIT_B                   = 0x00000066,
    
    // Camera
    RGB_IMAGE                = 0x00000010,
    VIDEO_STREAM             = 0x00000011,
    PIXEL_MATRIX             = 0x00000012,
    CAMERA_METADATA          = 0x00000013,

    // ECU
    SENSOR_DATA              = 0x00000101,
    DIAGNOSTIC_DATA          = 0x00000102,

    // INS
    ACCELERATION_VECTOR      = 0x00000201,
    GYRO_VECTOR              = 0x00000202,
    ORIENTATION_DATA         = 0x00000203,
    INERTIAL_POSITION        = 0x00000204,

    // Lidar
    POINT_CLOUD_XYZ          = 0x00000301,
    POINT_CLOUD_INTENSITY    = 0x00000302,
    LIDAR_TIMESTAMPED_SCAN   = 0x00000303,

    /* External (bit 31 = 1) */

    // Simple test units
    EXTERNAL_UNIT_A                   = 0x80000001,
    EXTERNAL_UNIT_B                   = 0x80000002,

    // Camera
    EXTERNAL_RGB_IMAGE                = 0x80000010,
    EXTERNAL_VIDEO_STREAM             = 0x80000011,
    EXTERNAL_PIXEL_MATRIX             = 0x80000012,
    EXTERNAL_CAMERA_METADATA          = 0x80000013,

    // ECU
    EXTERNAL_SENSOR_DATA              = 0x80000101,
    EXTERNAL_DIAGNOSTIC_DATA          = 0x80000102,

    // INS
    EXTERNAL_ACCELERATION_VECTOR      = 0x80000201,
    EXTERNAL_GYRO_VECTOR              = 0x80000202,
    EXTERNAL_ORIENTATION_DATA         = 0x80000203,
    EXTERNAL_INERTIAL_POSITION        = 0x80000204,

    // Lidar
    EXTERNAL_POINT_CLOUD_XYZ          = 0x80000301,
    EXTERNAL_POINT_CLOUD_INTENSITY    = 0x80000302,
    EXTERNAL_LIDAR_TIMESTAMPED_SCAN   = 0x80000303,

    // CSV Data
    CSV_VEHICLE_DATA                  = 0x00000401,
    EXTERNAL_CSV_VEHICLE_DATA         = 0x80000401
};



#endif // DATATYPES_H