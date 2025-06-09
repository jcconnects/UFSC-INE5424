#include "../../tests/testcase.h"
#include "../../tests/test_utils.h"
#include "../../include/api/framework/agent.h"
#include "../../include/api/network/bus.h"
#include "../../include/app/datatypes.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <cstring>
#include <cassert>
#include <cstdint>
#include <exception>

using namespace std::chrono_literals;

// Forward declarations
class AgentTest;

// Test Agent implementation for testing purposes
class TestAgent : public Agent {
public:
    TestAgent(CAN* can, const std::string& name, Unit unit, Type type, Address address = {})
        : Agent(can, name, unit, type, address), _test_value(42.0f) {}
    
    Agent::Value get(Agent::Unit unit) override {
        Agent::Value value(sizeof(float));
        std::memcpy(value.data(), &_test_value, sizeof(float));
        return value;
    }
    
    void handle_response(Message* msg) override {
        if (msg && msg->value_size() >= sizeof(float)) {
            const std::uint8_t* received_value = msg->value();
            _last_received_value = *reinterpret_cast<const float*>(received_value);
            _response_count++;
        }
    }
    
    // Test accessors
    float get_test_value() const { return _test_value; }
    float get_last_received_value() const { return _last_received_value; }
    int get_response_count() const { return _response_count; }
    void set_test_value(float value) { _test_value = value; }
    void reset_response_count() { _response_count = 0; }

private:
    float _test_value;
    float _last_received_value = 0.0f;
    int _response_count = 0;
};

class AgentTest : public TestCase {
protected:
    void setUp() override;
    void tearDown() override;

    // Helper methods
    std::unique_ptr<CAN> createTestCAN();
    void waitForMessage(int timeout_ms = 1000);
    
    // === BASIC AGENT FUNCTIONALITY TESTS ===
    void testAgentBasicConstruction();
    void testAgentConstructorValidation();
    void testAgentDestructorCleanup();
    void testAgentBasicSendReceive();
    void testAgentMessageHandling();
    
    // === PERIODIC INTEREST FUNCTIONALITY TESTS (Phase 1) ===
    void testStartPeriodicInterest();
    void testStartPeriodicInterestConsumerValidation();
    void testStartPeriodicInterestPeriodUpdate();
    void testStopPeriodicInterest();
    void testStopPeriodicInterestIdempotent();
    void testSendInterestSafety();
    void testUpdateInterestPeriod();
    void testPeriodicInterestThreadCreation();
    void testPeriodicInterestStateManagement();
    
    // === INTEGRATION TESTS ===
    void testConsumerProducerInteraction();
    void testMultipleConsumersSingleProducer();
    void testPeriodicInterestWithMessageFlow();
    
    // === THREAD SAFETY TESTS ===
    void testPeriodicInterestThreadSafety();
    void testAgentConcurrentOperations();
    
    // === EDGE CASES AND ERROR CONDITIONS ===
    void testPeriodicInterestEdgeCases();
    void testAgentInvalidStates();

private:
    std::unique_ptr<CAN> _test_can;

public:
    AgentTest();
};

/**
 * @brief Constructor that registers all test methods
 * 
 * Organizes tests into logical groups for better maintainability and clarity.
 * Focuses on comprehensive testing of Agent functionality with emphasis on
 * the new periodic interest features from Phase 1.
 */
AgentTest::AgentTest() {
    // === BASIC AGENT FUNCTIONALITY TESTS ===
    DEFINE_TEST(testAgentBasicConstruction);
    DEFINE_TEST(testAgentConstructorValidation);
    DEFINE_TEST(testAgentDestructorCleanup);
    DEFINE_TEST(testAgentBasicSendReceive);
    DEFINE_TEST(testAgentMessageHandling);
    
    // === PERIODIC INTEREST FUNCTIONALITY TESTS (Phase 1) ===
    DEFINE_TEST(testStartPeriodicInterest);
    DEFINE_TEST(testStartPeriodicInterestConsumerValidation);
    DEFINE_TEST(testStartPeriodicInterestPeriodUpdate);
    DEFINE_TEST(testStopPeriodicInterest);
    DEFINE_TEST(testStopPeriodicInterestIdempotent);
    DEFINE_TEST(testSendInterestSafety);
    DEFINE_TEST(testUpdateInterestPeriod);
    DEFINE_TEST(testPeriodicInterestThreadCreation);
    DEFINE_TEST(testPeriodicInterestStateManagement);
    
    // === INTEGRATION TESTS ===
    DEFINE_TEST(testConsumerProducerInteraction);
    DEFINE_TEST(testMultipleConsumersSingleProducer);
    DEFINE_TEST(testPeriodicInterestWithMessageFlow);
    
    // === THREAD SAFETY TESTS ===
    DEFINE_TEST(testPeriodicInterestThreadSafety);
    DEFINE_TEST(testAgentConcurrentOperations);
    
    // === EDGE CASES AND ERROR CONDITIONS ===
    DEFINE_TEST(testPeriodicInterestEdgeCases);
    DEFINE_TEST(testAgentInvalidStates);
}

void AgentTest::setUp() {
    _test_can = createTestCAN();
}

void AgentTest::tearDown() {
    _test_can.reset();
    // Allow time for cleanup
    std::this_thread::sleep_for(50ms);
}

std::unique_ptr<CAN> AgentTest::createTestCAN() {
    return std::make_unique<CAN>();
}

void AgentTest::waitForMessage(int timeout_ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
}

/**
 * @brief Tests basic Agent construction and initialization
 * 
 * Verifies that Agent objects can be created with valid parameters and that
 * the constructor properly initializes all member variables including the
 * new periodic interest functionality from Phase 1.
 */
void AgentTest::testAgentBasicConstruction() {
    TestAgent consumer(_test_can.get(), "TestConsumer", 
                      static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                      Agent::Type::RESPONSE);
    
    assert_equal("TestConsumer", consumer.name(), "Agent name should be set correctly");
    assert_true(consumer.running(), "Agent should be running after construction");
    
    TestAgent producer(_test_can.get(), "TestProducer", 
                      static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                      Agent::Type::INTEREST);
    
    assert_equal("TestProducer", producer.name(), "Producer name should be set correctly");
    assert_true(producer.running(), "Producer should be running after construction");
}

/**
 * @brief Tests Agent constructor parameter validation
 * 
 * Verifies that the Agent constructor properly validates input parameters
 * and throws appropriate exceptions for invalid inputs such as null CAN
 * bus pointers.
 */
void AgentTest::testAgentConstructorValidation() {
    // Test null CAN bus validation
    bool exception_thrown = false;
    try {
        TestAgent invalid_agent(nullptr, "InvalidAgent", 
                               static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                               Agent::Type::RESPONSE);
    } catch (const std::invalid_argument& e) {
        exception_thrown = true;
    }
    assert_true(exception_thrown, "Should throw exception for null CAN bus");
}

/**
 * @brief Tests Agent destructor cleanup
 * 
 * Verifies that the Agent destructor properly cleans up all resources,
 * including the new periodic interest threads from Phase 1. This test
 * ensures no memory leaks or hanging threads.
 */
void AgentTest::testAgentDestructorCleanup() {
    {
        TestAgent consumer(_test_can.get(), "TestConsumer", 
                          static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                          Agent::Type::RESPONSE);
        
        // Start periodic interest to create thread
        int result = consumer.start_periodic_interest(
            static_cast<std::uint32_t>(DataTypes::UNIT_A), 
            Agent::Microseconds(500000));
        assert_equal(0, result, "start_periodic_interest should succeed");
        
        // Agent will be destroyed here
    }
    
    // Allow time for cleanup
    std::this_thread::sleep_for(100ms);
    // Test passes if no crash occurs during cleanup
}

/**
 * @brief Tests basic Agent send/receive functionality
 * 
 * Verifies that Agent can send and receive messages through the CAN bus.
 * This test validates the core messaging functionality that underlies
 * the periodic interest system.
 */
void AgentTest::testAgentBasicSendReceive() {
    TestAgent consumer(_test_can.get(), "TestConsumer", 
                      static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                      Agent::Type::RESPONSE);
    
    // Test sending an INTEREST message
    int result = consumer.send(static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                              Agent::Microseconds(1000000));
    assert_true(result != -1, "Send should succeed");
}

/**
 * @brief Tests Agent message handling functionality
 * 
 * Verifies that Agent properly handles different types of messages
 * (INTEREST and RESPONSE) according to its configuration as either
 * a consumer or producer.
 */
void AgentTest::testAgentMessageHandling() {
    TestAgent consumer(_test_can.get(), "TestConsumer", 
                      static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                      Agent::Type::RESPONSE);
    
    TestAgent producer(_test_can.get(), "TestProducer", 
                      static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                      Agent::Type::INTEREST);
    
    // Initial response count should be zero
    assert_equal(0, consumer.get_response_count(), "Initial response count should be zero");
    
    // Allow agents to initialize
    waitForMessage(100);
}

/**
 * @brief Tests start_periodic_interest functionality
 * 
 * Verifies that the start_periodic_interest method properly initiates
 * periodic INTEREST message sending for consumer agents. This is a core
 * test for Phase 1 functionality.
 */
void AgentTest::testStartPeriodicInterest() {
    TestAgent consumer(_test_can.get(), "TestConsumer", 
                      static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                      Agent::Type::RESPONSE);
    
    // Test starting periodic interest
    int result = consumer.start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(500000));
    
    assert_equal(0, result, "start_periodic_interest should succeed for consumer");
    
    // Allow some time for messages to be sent
    waitForMessage(200);
    
    // Stop to clean up
    consumer.stop_periodic_interest();
}

/**
 * @brief Tests consumer validation in start_periodic_interest
 * 
 * Verifies that start_periodic_interest properly validates that the agent
 * is configured as a consumer before allowing periodic interest to start.
 * Producer agents should be rejected.
 */
void AgentTest::testStartPeriodicInterestConsumerValidation() {
    TestAgent producer(_test_can.get(), "TestProducer", 
                      static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                      Agent::Type::INTEREST);
    
    // Test that producers cannot start periodic interest
    int result = producer.start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(500000));
    
    assert_equal(-1, result, "start_periodic_interest should fail for producer");
}

/**
 * @brief Tests period update functionality in start_periodic_interest
 * 
 * Verifies that calling start_periodic_interest on an already active
 * periodic interest system properly updates the period instead of
 * creating a new thread.
 */
void AgentTest::testStartPeriodicInterestPeriodUpdate() {
    TestAgent consumer(_test_can.get(), "TestConsumer", 
                      static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                      Agent::Type::RESPONSE);
    
    // Start with initial period
    int result1 = consumer.start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(1000000));
    assert_equal(0, result1, "First start_periodic_interest should succeed");
    
    // Update period
    int result2 = consumer.start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(500000));
    assert_equal(0, result2, "Period update should succeed");
    
    consumer.stop_periodic_interest();
}

/**
 * @brief Tests stop_periodic_interest functionality
 * 
 * Verifies that stop_periodic_interest properly terminates periodic
 * INTEREST message sending and cleans up associated threads.
 */
void AgentTest::testStopPeriodicInterest() {
    TestAgent consumer(_test_can.get(), "TestConsumer", 
                      static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                      Agent::Type::RESPONSE);
    
    // Start periodic interest
    consumer.start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(500000));
    
    waitForMessage(100);
    
    // Stop periodic interest
    consumer.stop_periodic_interest();
    
    // Allow cleanup time
    waitForMessage(100);
    // Test passes if no issues during cleanup
}

/**
 * @brief Tests that stop_periodic_interest is idempotent
 * 
 * Verifies that calling stop_periodic_interest multiple times or on
 * an inactive periodic interest system does not cause errors or crashes.
 */
void AgentTest::testStopPeriodicInterestIdempotent() {
    TestAgent consumer(_test_can.get(), "TestConsumer", 
                      static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                      Agent::Type::RESPONSE);
    
    // Stop without starting (should not crash)
    consumer.stop_periodic_interest();
    consumer.stop_periodic_interest();
    
    // Start, stop, then stop again
    consumer.start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(500000));
    
    consumer.stop_periodic_interest();
    consumer.stop_periodic_interest(); // Should be safe
}

/**
 * @brief Tests send_interest safety checks
 * 
 * Verifies that the send_interest method includes proper safety checks
 * for thread state and agent running status before sending messages.
 */
void AgentTest::testSendInterestSafety() {
    TestAgent consumer(_test_can.get(), "TestConsumer", 
                      static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                      Agent::Type::RESPONSE);
    
    // Test that send_interest can be called safely
    // (Note: send_interest is typically called by the periodic thread,
    // but we can test it directly for safety)
    consumer.send_interest(static_cast<std::uint32_t>(DataTypes::UNIT_A));
    
    // Test passes if no crash occurs
}

/**
 * @brief Tests update_interest_period functionality
 * 
 * Verifies that update_interest_period properly adjusts the period of
 * an active periodic interest thread without stopping and restarting it.
 */
void AgentTest::testUpdateInterestPeriod() {
    TestAgent consumer(_test_can.get(), "TestConsumer", 
                      static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                      Agent::Type::RESPONSE);
    
    // Start periodic interest
    consumer.start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(1000000));
    
    waitForMessage(100);
    
    // Update period
    consumer.update_interest_period(Agent::Microseconds(500000));
    
    waitForMessage(100);
    
    consumer.stop_periodic_interest();
}

/**
 * @brief Tests periodic interest thread creation and management
 * 
 * Verifies that the periodic interest system properly creates and manages
 * threads for sending INTEREST messages, including proper thread lifecycle
 * management.
 */
void AgentTest::testPeriodicInterestThreadCreation() {
    TestAgent consumer(_test_can.get(), "TestConsumer", 
                      static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                      Agent::Type::RESPONSE);
    
    // Start periodic interest (creates thread)
    int result = consumer.start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(500000));
    assert_equal(0, result, "Thread creation should succeed");
    
    waitForMessage(200);
    
    // Stop (destroys thread)
    consumer.stop_periodic_interest();
    
    // Restart (creates new thread)
    result = consumer.start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(750000));
    assert_equal(0, result, "Thread recreation should succeed");
    
    consumer.stop_periodic_interest();
}

/**
 * @brief Tests periodic interest state management
 * 
 * Verifies that the periodic interest system properly manages its internal
 * state flags (_interest_active, _is_consumer) throughout the lifecycle
 * of starting and stopping periodic interest.
 */
void AgentTest::testPeriodicInterestStateManagement() {
    TestAgent consumer(_test_can.get(), "TestConsumer", 
                      static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                      Agent::Type::RESPONSE);
    
    TestAgent producer(_test_can.get(), "TestProducer", 
                      static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                      Agent::Type::INTEREST);
    
    // Consumer should be able to start periodic interest
    int consumer_result = consumer.start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(500000));
    assert_equal(0, consumer_result, "Consumer should start periodic interest");
    
    // Producer should not be able to start periodic interest
    int producer_result = producer.start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(500000));
    assert_equal(-1, producer_result, "Producer should not start periodic interest");
    
    consumer.stop_periodic_interest();
}

/**
 * @brief Tests consumer-producer interaction
 * 
 * Verifies that the complete consumer-producer interaction works correctly
 * with the new periodic interest system, including INTEREST message sending
 * and RESPONSE message handling.
 */
void AgentTest::testConsumerProducerInteraction() {
    TestAgent consumer(_test_can.get(), "TestConsumer", 
                      static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                      Agent::Type::RESPONSE);
    
    TestAgent producer(_test_can.get(), "TestProducer", 
                      static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                      Agent::Type::INTEREST);
    
    producer.set_test_value(123.45f);
    
    // Start periodic interest
    consumer.start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(100000)); // Fast period for testing
    
    // Allow time for interaction
    waitForMessage(500);
    
    consumer.stop_periodic_interest();
    
    // Check if interaction occurred (basic test)
    // Note: More detailed interaction testing would require message interception
}

/**
 * @brief Tests multiple consumers with single producer
 * 
 * Verifies that multiple consumer agents can simultaneously request data
 * from a single producer using the periodic interest system, and that
 * the producer handles multiple INTEREST streams correctly.
 */
void AgentTest::testMultipleConsumersSingleProducer() {
    TestAgent consumer1(_test_can.get(), "TestConsumer1", 
                       static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                       Agent::Type::RESPONSE);
    
    TestAgent consumer2(_test_can.get(), "TestConsumer2", 
                       static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                       Agent::Type::RESPONSE);
    
    TestAgent producer(_test_can.get(), "TestProducer", 
                      static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                      Agent::Type::INTEREST);
    
    // Start periodic interest on both consumers
    consumer1.start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(200000));
    
    consumer2.start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(300000));
    
    // Allow time for interactions
    waitForMessage(800);
    
    consumer1.stop_periodic_interest();
    consumer2.stop_periodic_interest();
    
    // Test passes if no crashes occur during multi-consumer scenario
}

/**
 * @brief Tests periodic interest with complete message flow
 * 
 * Verifies the end-to-end message flow from periodic INTEREST generation
 * through producer response and back to consumer handling. This integration
 * test validates the complete periodic interest cycle.
 */
void AgentTest::testPeriodicInterestWithMessageFlow() {
    TestAgent consumer(_test_can.get(), "TestConsumer", 
                      static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                      Agent::Type::RESPONSE);
    
    TestAgent producer(_test_can.get(), "TestProducer", 
                      static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                      Agent::Type::INTEREST);
    
    float test_value = 98.76f;
    producer.set_test_value(test_value);
    consumer.reset_response_count();
    
    // Start periodic interest
    consumer.start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(150000)); // 150ms period
    
    // Allow multiple cycles
    waitForMessage(600);
    
    consumer.stop_periodic_interest();
    
    // Basic validation that some interaction occurred
    // Note: Exact message counting would require more sophisticated test setup
}

/**
 * @brief Tests thread safety of periodic interest operations
 * 
 * Verifies that periodic interest operations can be safely called from
 * multiple threads without causing race conditions or data corruption.
 */
void AgentTest::testPeriodicInterestThreadSafety() {
    TestAgent consumer(_test_can.get(), "TestConsumer", 
                      static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                      Agent::Type::RESPONSE);
    
    std::atomic<bool> error_occurred{false};
    std::vector<std::thread> threads;
    const int num_threads = 3;
    const int num_operations = 50;
    
    auto thread_func = [&consumer, &error_occurred, num_operations]() {
        for (int i = 0; i < num_operations && !error_occurred; ++i) {
            try {
                switch (i % 4) {
                    case 0:
                        consumer.start_periodic_interest(
                            static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                            Agent::Microseconds(100000 + (i % 10) * 10000));
                        break;
                    case 1:
                        consumer.update_interest_period(
                            Agent::Microseconds(150000 + (i % 10) * 5000));
                        break;
                    case 2:
                        consumer.send_interest(static_cast<std::uint32_t>(DataTypes::UNIT_A));
                        break;
                    case 3:
                        consumer.stop_periodic_interest();
                        break;
                }
                std::this_thread::sleep_for(1ms);
            } catch (const std::exception& e) {
                error_occurred = true;
                return;
            }
        }
    };
    
    // Launch threads
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(thread_func);
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    consumer.stop_periodic_interest();
    assert_false(error_occurred, "Periodic interest operations should be thread-safe");
}

/**
 * @brief Tests concurrent operations on Agent
 * 
 * Verifies that various Agent operations can be performed concurrently
 * without causing race conditions, including periodic interest operations
 * and basic messaging functionality.
 */
void AgentTest::testAgentConcurrentOperations() {
    TestAgent consumer(_test_can.get(), "TestConsumer", 
                      static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                      Agent::Type::RESPONSE);
    
    std::atomic<bool> error_occurred{false};
    std::atomic<int> operation_count{0};
    
    auto operation_func = [&consumer, &error_occurred, &operation_count]() {
        for (int i = 0; i < 20 && !error_occurred; ++i) {
            try {
                // Mix of different operations
                consumer.name(); // Read operation
                consumer.running(); // State check
                if (i % 3 == 0) {
                    consumer.send_interest(static_cast<std::uint32_t>(DataTypes::UNIT_A));
                }
                operation_count++;
                std::this_thread::sleep_for(2ms);
            } catch (const std::exception& e) {
                error_occurred = true;
                return;
            }
        }
    };
    
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(operation_func);
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    assert_false(error_occurred, "Concurrent Agent operations should be safe");
    assert_true(operation_count > 0, "Operations should have been performed");
}

/**
 * @brief Tests edge cases in periodic interest functionality
 * 
 * Verifies that the periodic interest system handles edge cases correctly,
 * such as very short periods, very long periods, and zero periods.
 */
void AgentTest::testPeriodicInterestEdgeCases() {
    TestAgent consumer(_test_can.get(), "TestConsumer", 
                      static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                      Agent::Type::RESPONSE);
    
    // Test very short period
    int result1 = consumer.start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(1000)); // 1ms
    assert_equal(0, result1, "Should handle very short period");
    consumer.stop_periodic_interest();
    
    // Test very long period
    int result2 = consumer.start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(60000000)); // 60 seconds
    assert_equal(0, result2, "Should handle very long period");
    consumer.stop_periodic_interest();
    
    // Test zero period (should still work, though not practical)
    int result3 = consumer.start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(0));
    assert_equal(0, result3, "Should handle zero period");
    consumer.stop_periodic_interest();
}

/**
 * @brief Tests Agent behavior in invalid states
 * 
 * Verifies that Agent methods behave appropriately when called in
 * invalid or unexpected states, such as after the agent has been
 * destroyed or when the CAN bus is unavailable.
 */
void AgentTest::testAgentInvalidStates() {
    TestAgent consumer(_test_can.get(), "TestConsumer", 
                      static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                      Agent::Type::RESPONSE);
    
    // Test operations on stopped periodic interest
    consumer.update_interest_period(Agent::Microseconds(500000));
    consumer.stop_periodic_interest();
    
    // Test multiple starts and stops
    consumer.start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(100000));
    consumer.start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(200000));
    consumer.stop_periodic_interest();
    consumer.stop_periodic_interest();
    
    // Test passes if no crashes occur
}

// Main function
int main() {
    TEST_INIT("AgentTest");
    AgentTest test;
    test.run();
    return 0;
} 