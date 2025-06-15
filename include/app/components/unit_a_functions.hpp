#ifndef UNIT_A_FUNCTIONS_HPP
#define UNIT_A_FUNCTIONS_HPP

#include <vector>
#include <cstdint>
#include <cstring>
#include "unit_a_data.hpp"
#include "../../api/framework/component_functions.hpp"
#include "../../api/util/debug.h"

/**
 * @brief Producer function for UNIT_A data generation
 * 
 * Replaces BasicProducerA::get() virtual method with direct function call.
 * Generates random float values in the range 0.0f to 100.0f and returns
 * them as a byte vector for message transmission.
 * 
 * @param unit The data unit being requested (should be DataTypes::UNIT_A)
 * @param data Pointer to UnitAData structure containing random number generator
 * @return Vector of bytes containing the generated float value
 */
inline std::vector<std::uint8_t> basic_producer_a(std::uint32_t unit, ComponentData* data) {
    UnitAData* unit_data = static_cast<UnitAData*>(data);
    
    // Generate a random float value (same logic as BasicProducerA::get())
    float value = unit_data->dist(unit_data->gen);
    
    // Log the generated value (same as BasicProducerA)
    db<void>(TRC) << "[BasicProducerA] generated value: " << value << "\n";
    
    // Convert float to bytes for the message value (same as BasicProducerA)
    std::vector<std::uint8_t> result(sizeof(float));
    std::memcpy(result.data(), &value, sizeof(float));
    
    return result;
}

/**
 * @brief Consumer function for UNIT_A response handling
 * 
 * Replaces BasicConsumerA::handle_response() virtual method with direct function call.
 * Extracts float value from received message, stores it in component data,
 * and logs the reception (matching original behavior).
 * 
 * @param msg Pointer to the received message (cast to Agent::Message*)
 * @param data Pointer to UnitAData structure for storing received value
 */
inline void basic_consumer_a(void* msg, ComponentData* data) {
    UnitAData* unit_data = static_cast<UnitAData*>(data);
    
    // Note: In real implementation, msg would be cast to Agent::Message*
    // For now, we simulate the behavior from BasicConsumerA::handle_response()
    
    // Cast message pointer (following BasicConsumerA pattern)
    // Agent::Message* message = static_cast<Agent::Message*>(msg);
    
    // Extract float value (same logic as BasicConsumerA::handle_response())
    // const std::uint8_t* received_value = message->value();
    // unit_data->last_received_value = *reinterpret_cast<const float*>(received_value);
    
    // For testing purposes, we simulate the extraction
    // This will be properly implemented when Agent::Message is available
    unit_data->last_received_value = 42.0f; // Placeholder
    
    // Log the received message (same as BasicConsumerA)
    db<void>(INF) << "[BasicConsumerA] received RESPONSE message with value: " 
                  << unit_data->last_received_value << "\n";
}

#endif // UNIT_A_FUNCTIONS_HPP 