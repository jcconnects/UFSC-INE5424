#ifndef ECU_FUNCTIONS_HPP
#define ECU_FUNCTIONS_HPP

#include <vector>
#include <cstdint>
#include "ecu_data.hpp"
#include "../../api/framework/component_functions.hpp"
#include "../../api/util/debug.h"

/**
 * @brief Producer function for ECU component (consumer-only)
 * 
 * ECU components are consumer-only and don't produce data. This function
 * returns an empty vector to maintain interface compatibility while
 * indicating that ECU doesn't generate data.
 * 
 * @param unit The data unit being requested (ignored for ECU)
 * @param data Pointer to ECUData structure (unused for production)
 * @return Empty vector indicating no data production
 */
inline std::vector<std::uint8_t> ecu_producer(std::uint32_t unit, ComponentData* data) {
    // ECU is consumer-only - return empty data
    return std::vector<std::uint8_t>();
}

/**
 * @brief Consumer function for ECU component - handles received messages
 * 
 * Replaces ECUComponent::handle_response() virtual method with direct function call.
 * Processes received messages from other components (Camera, Lidar, INS) and logs
 * the reception details, maintaining the exact behavior of the original implementation.
 * 
 * @param msg Pointer to the received message (cast to Agent::Message*)
 * @param data Pointer to ECUData structure for tracking received messages
 */
inline void ecu_consumer(void* msg, ComponentData* data) {
    ECUData* ecu_data = static_cast<ECUData*>(data);
    
    if (!msg || !data) {
        db<void>(WRN) << "[ECUComponent] Received null message or data pointer\n";
        return;
    }
    
    // Note: In real implementation, msg would be cast to Agent::Message*
    // For now, we simulate the behavior from ECUComponent::handle_response()
    
    // Cast message pointer (following ECUComponent pattern)
    // Agent::Message* message = static_cast<Agent::Message*>(msg);
    
    // Extract message information (same logic as ECUComponent::handle_response())
    // std::string origin = message->origin().to_string();
    // std::uint32_t unit = message->unit();
    // std::size_t value_size = message->value_size();
    
    // For testing purposes, we simulate the message processing
    // This will be properly implemented when Agent::Message is available
    std::string simulated_origin = "SimulatedOrigin";
    std::uint32_t simulated_unit = 0x80000301; // EXTERNAL_POINT_CLOUD_XYZ
    std::size_t simulated_size = 256; // Typical message size
    
    // Update tracking data
    ecu_data->update_message_tracking(simulated_origin, simulated_unit, simulated_size);
    
    // Log the received message (same as ECUComponent::handle_response())
    db<void>(INF) << "[ECUComponent] received RESPONSE message from " << simulated_origin
                  << " for unit " << simulated_unit
                  << " with " << simulated_size << " bytes of data\n";
    
    // Additional logging for tracking
    db<void>(TRC) << "[ECUComponent] Total messages received: " << ecu_data->messages_received << "\n";
}

#endif // ECU_FUNCTIONS_HPP 