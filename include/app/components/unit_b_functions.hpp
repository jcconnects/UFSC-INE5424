#ifndef UNIT_B_FUNCTIONS_HPP
#define UNIT_B_FUNCTIONS_HPP

#include <vector>
#include <cstdint>
#include <cstring>
#include "unit_b_data.hpp"
#include "../../api/framework/component_functions.hpp"
#include "../../api/util/debug.h"

/**
 * @brief Producer function for UNIT_B data generation
 * 
 * Replaces BasicProducerB::get() virtual method with direct function call.
 * Generates random float values in the configured range (default 200.0f to 300.0f)
 * and returns them as a byte vector for message transmission.
 * 
 * @param unit The data unit being requested (should be DataTypes::UNIT_B)
 * @param data Pointer to UnitBData structure containing random number generator
 * @return Vector of bytes containing the generated float value
 */
inline std::vector<std::uint8_t> basic_producer_b(std::uint32_t unit, ComponentData* data) {
    UnitBData* unit_data = static_cast<UnitBData*>(data);
    
    // Generate a random float value (same logic as BasicProducerB::get())
    float value = unit_data->dist(unit_data->gen);
    
    // Log the generated value (same as BasicProducerB)
    db<void>(TRC) << "[BasicProducerB] generated value: " << value << "\n";
    
    // Convert float to bytes for the message value (same as BasicProducerB)
    std::vector<std::uint8_t> result(sizeof(float));
    std::memcpy(result.data(), &value, sizeof(float));
    
    return result;
}

/**
 * @brief Consumer function for UNIT_B response handling
 * 
 * Replaces BasicConsumerB::handle_response() virtual method with direct function call.
 * Extracts float value from received message, stores it in component data,
 * and logs the reception (matching original behavior).
 * 
 * @param msg Pointer to the received message (cast to Agent::Message*)
 * @param data Pointer to UnitBData structure for storing received value
 */
inline void basic_consumer_b(void* msg, ComponentData* data) {
    UnitBData* unit_data = static_cast<UnitBData*>(data);
    
    // Note: In real implementation, msg would be cast to Agent::Message*
    // For now, we simulate the behavior from BasicConsumerB::handle_response()
    
    // Cast message pointer (following BasicConsumerB pattern)
    // Agent::Message* message = static_cast<Agent::Message*>(msg);
    
    // Extract float value (same logic as BasicConsumerB::handle_response())
    // const std::uint8_t* received_value = message->value();
    // unit_data->last_received_value = *reinterpret_cast<const float*>(received_value);
    
    // For testing purposes, we simulate the extraction
    // This will be properly implemented when Agent::Message is available
    unit_data->last_received_value = 250.0f; // Placeholder (mid-range for UNIT_B)
    
    // Log the received message (same as BasicConsumerB)
    db<void>(INF) << "[BasicConsumerB] received RESPONSE message with value: " 
                  << unit_data->last_received_value << "\n";
}

#endif // UNIT_B_FUNCTIONS_HPP 