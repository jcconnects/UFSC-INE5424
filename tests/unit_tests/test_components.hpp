#ifndef TEST_COMPONENTS_HPP
#define TEST_COMPONENTS_HPP

#include <cstring>
#include <stdexcept>

#include "api/framework/component_types.hpp"
#include "api/framework/component_functions.hpp"

/**
 * @brief Simple test component data for basic functionality testing
 * 
 * Minimal component data structure following EPOS SmartData principles.
 * Used for testing the function-based Agent architecture.
 */
struct SimpleTestComponent : public ComponentData {
    float value = 42.0f;
    int call_count = 0;
    bool should_throw = false;
    
    SimpleTestComponent(float initial_value = 42.0f) : value(initial_value) {}
    
    void reset() {
        call_count = 0;
        should_throw = false;
    }
};

/**
 * @brief Counter test component data for sequence testing
 * 
 * Component that increments a counter each time it's accessed.
 * Useful for testing periodic operations and call frequency.
 */
struct CounterTestComponent : public ComponentData {
    int counter = 0;
    float base_value = 100.0f;
    
    CounterTestComponent(float base = 100.0f) : base_value(base) {}
    
    void reset() {
        counter = 0;
    }
    
    float get_next_value() {
        return base_value + (++counter);
    }
};

/**
 * @brief Response tracking test component for consumer testing
 * 
 * Component that tracks received responses for validation.
 * Used for testing consumer functionality and message handling.
 */
struct ResponseTrackingComponent : public ComponentData {
    float last_received_value = 0.0f;
    int response_count = 0;
    bool should_throw = false;
    
    void reset() {
        last_received_value = 0.0f;
        response_count = 0;
        should_throw = false;
    }
    
    void record_response(float value) {
        last_received_value = value;
        response_count++;
    }
};

// === TEST PRODUCER FUNCTIONS ===

/**
 * @brief Simple producer function for basic testing
 * 
 * Returns a fixed float value from the component data.
 * Used for basic producer functionality testing.
 */
inline std::vector<std::uint8_t> simple_producer(std::uint32_t unit, ComponentData* data) {
    SimpleTestComponent* component = static_cast<SimpleTestComponent*>(data);
    
    if (component->should_throw) {
        throw std::runtime_error("Test exception in simple producer");
    }
    
    component->call_count++;
    
    std::vector<std::uint8_t> result(sizeof(float));
    std::memcpy(result.data(), &component->value, sizeof(float));
    return result;
}

/**
 * @brief Counter producer function for sequence testing
 * 
 * Returns incrementing values for testing periodic operations.
 * Each call returns a different value to verify call frequency.
 */
inline std::vector<std::uint8_t> counter_producer(std::uint32_t unit, ComponentData* data) {
    CounterTestComponent* component = static_cast<CounterTestComponent*>(data);
    
    float next_value = component->get_next_value();
    
    std::vector<std::uint8_t> result(sizeof(float));
    std::memcpy(result.data(), &next_value, sizeof(float));
    return result;
}

/**
 * @brief Null producer function for testing null pointer handling
 * 
 * Always returns empty data to test null/empty value handling.
 */
inline std::vector<std::uint8_t> null_producer(std::uint32_t unit, ComponentData* data) {
    return std::vector<std::uint8_t>(); // Return empty vector
}

// === TEST CONSUMER FUNCTIONS ===

/**
 * @brief Response tracking consumer function
 * 
 * Records received responses for validation in tests.
 * Used for testing consumer functionality and message handling.
 */
inline void response_tracker(void* msg, ComponentData* data) {
    ResponseTrackingComponent* component = static_cast<ResponseTrackingComponent*>(data);
    
    if (component->should_throw) {
        throw std::runtime_error("Test exception in response tracker");
    }
    
    // In real implementation, msg would be cast to Agent::Message*
    // For testing, we simulate the behavior
    component->response_count++;
    
    // Note: Actual message parsing would happen here in real implementation
    // For testing purposes, we just track that the function was called
}

/**
 * @brief Null consumer function for testing null pointer handling
 * 
 * Does nothing, used for testing null function pointer handling.
 */
inline void null_consumer(void* msg, ComponentData* data) {
    // Do nothing - used for null pointer testing
}

#endif // TEST_COMPONENTS_HPP 