#ifndef UNIT_B_DATA_HPP
#define UNIT_B_DATA_HPP

#include <random>
#include "../../api/framework/component_types.hpp"

/**
 * @brief Data structure for UNIT_B components
 * 
 * Following EPOS SmartData principles, this structure contains all the data
 * needed for both BasicProducerB and BasicConsumerB functionality.
 * Replaces inheritance-based data storage with pure composition.
 * 
 * Uses a different default range (200.0f to 300.0f) compared to UNIT_A
 * to provide clear differentiation between component types.
 */
struct UnitBData : public ComponentData {
    // Producer data: random number generation (from BasicProducerB)
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_real_distribution<float> dist;
    
    // Consumer data: tracking received values (from BasicConsumerB)
    float last_received_value;
    
    /**
     * @brief Constructor initializes random number generation and consumer state
     * 
     * Sets up random distribution with configurable range. Default range
     * matches BasicProducerB (200.0f to 300.0f) for backward compatibility.
     * 
     * @param min_range Minimum value for random generation (default: 200.0f)
     * @param max_range Maximum value for random generation (default: 300.0f)
     */
    UnitBData(float min_range = 200.0f, float max_range = 300.0f) 
        : gen(rd()), dist(min_range, max_range), last_received_value(0.0f) {}
    
    /**
     * @brief Reset consumer state for testing purposes
     */
    void reset_consumer_state() {
        last_received_value = 0.0f;
    }
    
    /**
     * @brief Update the random number generation range
     * 
     * Allows dynamic reconfiguration of the value generation range.
     * Useful for testing different scenarios or runtime configuration.
     * 
     * @param min_val New minimum value
     * @param max_val New maximum value
     */
    void update_range(float min_val, float max_val) {
        dist = std::uniform_real_distribution<float>(min_val, max_val);
    }
};

#endif // UNIT_B_DATA_HPP 