#ifndef CSV_CONSUMER_DATA_HPP
#define CSV_CONSUMER_DATA_HPP

#include <string>
#include <cstdint>
#include <chrono>
#include "../../api/framework/component_types.hpp"

/**
 * @brief Data structure for CSV consumer component
 * 
 * Following EPOS SmartData principles, this structure contains all the data
 * needed for CSV consumer functionality. Tracks received CSV vehicle data
 * messages and provides statistics about message reception.
 * 
 * CSV consumer processes messages with prepended timestamps and CSV records
 * in the format: timestamp,id,lat,lon,alt,x,y,z,speed,heading,yawrate,acceleration
 */
struct CSVConsumerData : public ComponentData {
    // Statistics tracking
    std::size_t messages_received;
    std::size_t total_bytes_received;
    std::size_t invalid_messages;
    
    // Latest received data tracking
    std::uint64_t last_csv_timestamp;
    std::uint32_t last_vehicle_id;
    double last_position_x;
    double last_position_y;
    double last_position_z;
    double last_speed;
    
    // Component timing
    std::chrono::steady_clock::time_point last_message_time;
    std::chrono::steady_clock::time_point start_time;
    
    /**
     * @brief Constructor initializes CSV consumer state
     * 
     * Sets up the CSV consumer with default values for tracking
     * received CSV vehicle data messages.
     */
    CSVConsumerData() : messages_received(0),
                       total_bytes_received(0),
                       invalid_messages(0),
                       last_csv_timestamp(0),
                       last_vehicle_id(0),
                       last_position_x(0.0),
                       last_position_y(0.0),
                       last_position_z(0.0),
                       last_speed(0.0),
                       start_time(std::chrono::steady_clock::now()) {}
    
    /**
     * @brief Update tracking data when a message is received
     * 
     * @param message_size Size of the received message in bytes
     * @param csv_timestamp Timestamp from the CSV record
     * @param vehicle_id Vehicle ID from the CSV record
     * @param x X position from CSV record
     * @param y Y position from CSV record
     * @param z Z position from CSV record
     * @param speed Speed from CSV record
     */
    void update_message_tracking(std::size_t message_size, std::uint64_t csv_timestamp,
                                std::uint32_t vehicle_id, double x, double y, double z, double speed) {
        messages_received++;
        total_bytes_received += message_size;
        last_csv_timestamp = csv_timestamp;
        last_vehicle_id = vehicle_id;
        last_position_x = x;
        last_position_y = y;
        last_position_z = z;
        last_speed = speed;
        last_message_time = std::chrono::steady_clock::now();
    }
    
    /**
     * @brief Mark a message as invalid
     */
    void mark_invalid_message() {
        invalid_messages++;
    }
    
    /**
     * @brief Get total runtime in seconds
     * 
     * @return Runtime since component creation in seconds
     */
    double get_runtime_seconds() const {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
        return duration.count();
    }
    
    /**
     * @brief Get message reception rate in messages per second
     * 
     * @return Messages per second since component creation
     */
    double get_message_rate() const {
        double runtime = get_runtime_seconds();
        return runtime > 0 ? static_cast<double>(messages_received) / runtime : 0.0;
    }
    
    /**
     * @brief Get data reception rate in bytes per second
     * 
     * @return Bytes per second since component creation
     */
    double get_data_rate() const {
        double runtime = get_runtime_seconds();
        return runtime > 0 ? static_cast<double>(total_bytes_received) / runtime : 0.0;
    }
};

#endif // CSV_CONSUMER_DATA_HPP 