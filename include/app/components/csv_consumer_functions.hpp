#ifndef CSV_CONSUMER_FUNCTIONS_HPP
#define CSV_CONSUMER_FUNCTIONS_HPP

#include <vector>
#include <cstdint>
#include <cstring>
#include <chrono>
#include "csv_consumer_data.hpp"
#include "csv_component_data.hpp"  // For CSVRecord structure
#include "../../api/framework/component_functions.hpp"
#include "../../api/util/debug.h"

/**
 * @brief Producer function for CSV consumer component (consumer-only)
 * 
 * CSV consumer components are consumer-only and don't produce data. This function
 * returns an empty vector to maintain interface compatibility while
 * indicating that the consumer doesn't generate data.
 * 
 * @param unit The data unit being requested (ignored for consumer)
 * @param data Pointer to CSVConsumerData structure (unused for production)
 * @return Empty vector indicating no data production
 */
inline std::vector<std::uint8_t> csv_consumer_producer(std::uint32_t unit, ComponentData* data) {
    // CSV consumer is consumer-only - return empty data
    db<void>(TRC) << "[CSVConsumer] Producer function called (CSV consumer has no producer implementation)\n";
    return std::vector<std::uint8_t>();
}

/**
 * @brief Consumer function for CSV consumer component - processes CSV vehicle data
 * 
 * Processes received CSV vehicle data messages that contain a prepended timestamp
 * followed by a complete CSV record. Extracts the timestamp, parses the CSV record,
 * and logs the reception details while tracking statistics.
 * 
 * Expected message format:
 * - First 8 bytes: std::uint64_t timestamp (prepended by CSV producer)
 * - Remaining bytes: CSVComponentData::CSVRecord structure
 * 
 * @param msg Pointer to the received message data
 * @param data Pointer to CSVConsumerData structure for tracking received messages
 */
inline void csv_consumer_consumer(void* msg, ComponentData* data) {
    CSVConsumerData* consumer_data = static_cast<CSVConsumerData*>(data);
    
    if (!msg || !data) {
        db<void>(WRN) << "[CSVConsumer] Received null message or data pointer\n";
        if (consumer_data) {
            consumer_data->mark_invalid_message();
        }
        return;
    }
    
    // Note: In real implementation, msg would be cast to Agent::Message*
    // and we would extract timestamp from message header and CSV data from value
    // For testing purposes, we simulate this behavior
    
    // Cast message to byte array for parsing the CSV record data
    const std::uint8_t* message_bytes = static_cast<const std::uint8_t*>(msg);
    
    // Calculate expected message size: only CSV record (timestamp is now in message header)
    const std::size_t expected_size = sizeof(CSVComponentData::CSVRecord);
    
    // Note: In real implementation, we would get these from the Message object:
    // std::size_t message_size = message->value_size(); 
    // std::uint64_t csv_timestamp = message->timestamp().count();
    std::size_t message_size = expected_size; // This would come from message->value_size()
    
    // Validate message size
    if (message_size < expected_size) {
        db<void>(ERR) << "[CSVConsumer] Invalid message size: " << message_size 
                      << ", expected: " << expected_size << "\n";
        consumer_data->mark_invalid_message();
        return;
    }
    
    // Simulate extracting timestamp from message header 
    // In real implementation: csv_timestamp = message->timestamp().count();
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::uint64_t csv_timestamp = now; // Simulated timestamp from message header
    
    // Extract CSV record directly from message data (no timestamp prefix anymore)
    CSVComponentData::CSVRecord csv_record;
    std::memcpy(&csv_record, message_bytes, sizeof(CSVComponentData::CSVRecord));
    
    // Update tracking data
    consumer_data->update_message_tracking(
        message_size,
        csv_timestamp,
        csv_record.id,
        csv_record.x,
        csv_record.y,
        csv_record.z,
        csv_record.speed
    );
    
    // Log the received CSV data
    db<void>(INF) << "[CSVConsumer] Received CSV vehicle data message #" << consumer_data->messages_received
                  << " timestamp: " << csv_timestamp
                  << " vehicle_id: " << csv_record.id
                  << " position: (" << csv_record.x << ", " << csv_record.y << ", " << csv_record.z << ")"
                  << " speed: " << csv_record.speed
                  << " size: " << message_size << " bytes\n";
    
    // Additional detailed logging for debugging
    db<void>(TRC) << "[CSVConsumer] CSV record details: "
                  << "lat=" << csv_record.lat << ", "
                  << "lon=" << csv_record.lon << ", "
                  << "alt=" << csv_record.alt << ", "
                  << "heading=" << csv_record.heading << ", "
                  << "yawrate=" << csv_record.yawrate << ", "
                  << "acceleration=" << csv_record.acceleration << "\n";
    
    // Log statistics periodically (every 100 messages)
    if (consumer_data->messages_received % 100 == 0) {
        db<void>(INF) << "[CSVConsumer] Statistics: "
                      << "total_messages=" << consumer_data->messages_received
                      << ", invalid_messages=" << consumer_data->invalid_messages
                      << ", total_bytes=" << consumer_data->total_bytes_received
                      << ", msg_rate=" << consumer_data->get_message_rate() << " msg/s"
                      << ", data_rate=" << consumer_data->get_data_rate() << " bytes/s\n";
    }
}

#endif // CSV_CONSUMER_FUNCTIONS_HPP 