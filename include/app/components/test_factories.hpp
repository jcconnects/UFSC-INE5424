#ifndef TEST_FACTORIES_HPP
#define TEST_FACTORIES_HPP

#include <memory>
#include <stdexcept>
#include <cstring>
#include "../../api/framework/agent.h"
#include "../../api/framework/component_types.hpp"
#include "../../api/framework/component_functions.hpp"
#include "../datatypes.h"

/**
 * @brief Test-specific component data structure for predictable testing
 * 
 * Provides the same interface as the original TestAgent class but using
 * the EPOS SmartData composition approach. This eliminates vtable race
 * conditions while maintaining exact test behavior.
 */
struct TestComponentData : public ComponentData {
    float test_value = 42.0f;           // Fixed predictable value (same as original TestAgent)
    float last_received_value = 0.0f;   // Track received values
    int response_count = 0;              // Count responses
    bool should_throw = false;           // For exception testing
    
    // Test-specific methods for verification (same interface as original TestAgent)
    float get_test_value() const { return test_value; }
    float get_last_received_value() const { return last_received_value; }
    int get_response_count() const { return response_count; }
    void set_test_value(float value) { test_value = value; }
    void reset_response_count() { response_count = 0; }
    void set_should_throw(bool throw_flag) { should_throw = throw_flag; }
};

/**
 * @brief Test producer function that returns predictable values
 * 
 * Replaces the virtual get() method from TestAgent with a function pointer.
 * Always returns the fixed test_value (42.0f by default) for predictable testing.
 * 
 * @param unit The data unit being requested
 * @param data Pointer to TestComponentData
 * @return Value containing the test data
 */
inline std::vector<std::uint8_t> test_producer_function(std::uint32_t unit, ComponentData* data) {
    TestComponentData* test_data = static_cast<TestComponentData*>(data);
    
    if (test_data->should_throw) {
        throw std::runtime_error("Test exception in producer function");
    }
    
    std::vector<std::uint8_t> value(sizeof(float));
    std::memcpy(value.data(), &test_data->test_value, sizeof(float));
    return value;
}

/**
 * @brief Test consumer function that tracks received responses
 * 
 * Replaces the virtual handle_response() method from TestAgent with a function pointer.
 * Tracks received values and response counts for test verification.
 * 
 * @param msg Pointer to the received message
 * @param data Pointer to TestComponentData
 */
inline void test_consumer_function(void* msg, ComponentData* data) {
    TestComponentData* test_data = static_cast<TestComponentData*>(data);
    
    if (test_data->should_throw) {
        throw std::runtime_error("Test exception in consumer function");
    }
    
    if (!msg) return; // Safety check
    
    // Cast to Agent::Message* for proper message handling
    Agent::Message* message = static_cast<Agent::Message*>(msg);
    
    if (message && message->value_size() >= sizeof(float)) {
        const std::uint8_t* received_value = message->value();
        test_data->last_received_value = *reinterpret_cast<const float*>(received_value);
        test_data->response_count++;
    }
}

/**
 * @brief Creates a test producer agent with predictable behavior
 * 
 * Replaces the inheritance-based TestAgent producer with function-based composition.
 * Provides the same predictable test behavior (fixed 42.0f values) as the original.
 * 
 * @param can CAN bus for communication (must not be null)
 * @param addr Network address for the agent
 * @param name Agent name for identification (default: "TestProducer")
 * @param test_value Initial test value (default: 42.0f)
 * @return Unique pointer to configured test producer Agent
 * @throws std::invalid_argument if can is null or name is empty
 */
inline std::unique_ptr<Agent> create_test_producer(
    CAN* can, 
    const Agent::Address& addr, 
    const std::string& name = "TestProducer",
    float test_value = 42.0f
) {
    if (!can) {
        throw std::invalid_argument("CAN bus cannot be null");
    }
    
    if (name.empty()) {
        throw std::invalid_argument("Agent name cannot be empty");
    }
    
    auto data = std::make_unique<TestComponentData>();
    data->set_test_value(test_value);
    
    return std::make_unique<Agent>(
        can, name,
        static_cast<std::uint32_t>(DataTypes::UNIT_A),
        Agent::Type::INTEREST, // Producer observes INTEREST messages
        addr,
        test_producer_function,
        nullptr, // Producers don't need response handlers
        std::move(data)
    );
}

/**
 * @brief Creates a test consumer agent with response tracking
 * 
 * Replaces the inheritance-based TestAgent consumer with function-based composition.
 * Provides the same response tracking capabilities as the original TestAgent.
 * 
 * @param can CAN bus for communication (must not be null)
 * @param addr Network address for the agent
 * @param name Agent name for identification (default: "TestConsumer")
 * @return Unique pointer to configured test consumer Agent
 * @throws std::invalid_argument if can is null or name is empty
 */
inline std::unique_ptr<Agent> create_test_consumer(
    CAN* can, 
    const Agent::Address& addr, 
    const std::string& name = "TestConsumer"
) {
    if (!can) {
        throw std::invalid_argument("CAN bus cannot be null");
    }
    
    if (name.empty()) {
        throw std::invalid_argument("Agent name cannot be empty");
    }
    
    auto data = std::make_unique<TestComponentData>();
    
    return std::make_unique<Agent>(
        can, name,
        static_cast<std::uint32_t>(DataTypes::UNIT_A),
        Agent::Type::RESPONSE, // Consumer observes RESPONSE messages
        addr,
        nullptr, // Consumers don't need producer functions
        test_consumer_function,
        std::move(data)
    );
}

/**
 * @brief Creates a test consumer with automatic periodic interest
 * 
 * Convenience function that creates a test consumer and automatically starts
 * periodic interest for the specified period.
 * 
 * @param can CAN bus for communication (must not be null)
 * @param addr Network address for the agent
 * @param period Period for automatic periodic interest
 * @param name Agent name for identification (default: "TestConsumer")
 * @return Unique pointer to configured test consumer Agent with active periodic interest
 * @throws std::invalid_argument if can is null or name is empty
 */
inline std::unique_ptr<Agent> create_test_consumer_with_period(
    CAN* can, 
    const Agent::Address& addr, 
    Agent::Microseconds period,
    const std::string& name = "TestConsumer"
) {
    auto consumer = create_test_consumer(can, addr, name);
    consumer->start_periodic_interest(static_cast<std::uint32_t>(DataTypes::UNIT_A), period);
    return consumer;
}

/**
 * @brief Helper function to access TestComponentData from Agent
 * 
 * Provides a way to access the test-specific data for verification in tests.
 * This is a workaround since we can't directly access private component data.
 * 
 * Note: This is a test-only helper and should not be used in production code.
 * 
 * @param agent Pointer to the test agent
 * @return Pointer to TestComponentData (nullptr if not a test agent)
 */
inline TestComponentData* get_test_data(Agent* agent) {
    // Note: This is a simplified approach for testing
    // In a real implementation, we might need a more sophisticated way
    // to access component data for verification
    return nullptr; // Placeholder - actual implementation would need Agent API changes
}

#endif // TEST_FACTORIES_HPP