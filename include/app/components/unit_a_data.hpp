#ifndef UNIT_A_DATA_HPP
#define UNIT_A_DATA_HPP

#include <random>
#include "../../api/framework/component_types.hpp"

/**
 * @brief Data structure for UNIT_A components
 * 
 * Following EPOS SmartData principles, this structure contains all the data
 * needed for both BasicProducerA and BasicConsumerA functionality.
 * Replaces inheritance-based data storage with pure composition.
 */
struct UnitAData : public ComponentData {
    // Producer data: random number generation (from BasicProducerA)
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_real_distribution<float> dist;
    
    // Consumer data: tracking received values (from BasicConsumerA)
    float last_received_value;
    
    /**
     * @brief Constructor initializes random number generation and consumer state
     * 
     * Sets up the same random distribution as BasicProducerA (0.0f to 100.0f)
     * and initializes consumer tracking variables.
     */
    UnitAData() : gen(rd()), dist(0.0f, 100.0f), last_received_value(0.0f) {}
    
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

#endif // UNIT_A_DATA_HPP 