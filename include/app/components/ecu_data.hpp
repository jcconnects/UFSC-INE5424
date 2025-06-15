#ifndef ECU_DATA_HPP
#define ECU_DATA_HPP

#include "../../api/framework/component_types.hpp"
#include <string>

/**
 * @brief Data structure for ECU component
 * 
 * Following EPOS SmartData principles, this structure contains all the data
 * needed for ECUComponent functionality. ECU is a consumer-only component
 * that receives and processes messages from other components (Camera, Lidar, INS).
 * 
 * Replaces inheritance-based data storage with pure composition.
 */
struct ECUData : public ComponentData {
    // Consumer data: tracking received messages
    int messages_received;
    std::string last_message_source;
    std::uint32_t last_message_unit;
    std::size_t last_message_size;
    
    /**
     * @brief Constructor initializes consumer tracking state
     * 
     * Sets up initial state for message tracking and processing.
     * ECU components don't need producer data since they are consumer-only.
     */
    ECUData() : messages_received(0), last_message_source(""), 
                last_message_unit(0), last_message_size(0) {}
    
    /**
     * @brief Reset consumer state for testing purposes
     */
    void reset_consumer_state() {
        messages_received = 0;
        last_message_source = "";
        last_message_unit = 0;
        last_message_size = 0;
    }
    
    /**
     * @brief Update message tracking with new received message
     * 
     * @param source Source component name/address
     * @param unit Data unit type received
     * @param size Message size in bytes
     */
    void update_message_tracking(const std::string& source, std::uint32_t unit, std::size_t size) {
        messages_received++;
        last_message_source = source;
        last_message_unit = unit;
        last_message_size = size;
    }
};

#endif // ECU_DATA_HPP 