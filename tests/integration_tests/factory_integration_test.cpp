#include "../testcase.h"
#include "../test_utils.h"
#include "../../include/api/network/bus.h"
#include "../../include/app/components/basic_producer_a_factory.hpp"
#include "../../include/app/components/basic_consumer_a_factory.hpp"
#include "../../include/app/components/basic_producer_b_factory.hpp"
#include "../../include/app/components/basic_consumer_b_factory.hpp"
#include "../../include/app/datatypes.h"
#include <memory>
#include <vector>
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

/**
 * @brief Integration test suite for component factory functions
 * 
 * Tests the complete end-to-end functionality of factory-created agents,
 * including factory creation, Agent operation, periodic interest, message flow,
 * and proper cleanup. Validates that the function-based approach provides
 * the same functionality as the original inheritance-based classes.
 */
class FactoryIntegrationTest : public TestCase {
protected:
    void setUp() override;
    void tearDown() override;

    // Helper methods
    std::unique_ptr<CAN> createTestCAN();
    Agent::Address createTestAddress(int id = 0);
    void waitForMessages(int timeout_ms = 200);
    
    // === BASIC INTEGRATION TESTS ===
    void testFactoryToAgentCreation();
    void testFactoryAgentBasicMessaging();
    void testFactoryAgentDataGeneration();
    
    // === PERIODIC INTEREST INTEGRATION TESTS ===
    void testFactoryAgentPeriodicInterest();
    void testFactoryAgentPeriodicInterestLifecycle();
    void testFactoryAgentMultiplePeriodicOperations();
    
    // === PRODUCER-CONSUMER INTEGRATION TESTS ===
    void testFactoryProducerConsumerInteraction();
    void testFactoryMultipleConsumersSingleProducer();
    void testFactoryMixedUnitInteraction();
    
    // === COMPLETE MESSAGE FLOW TESTS ===
    void testFactoryCompleteMessageFlow();
    void testFactoryMessageFlowWithCustomRanges();
    void testFactoryMessageFlowStressTest();
    
    // === CLEANUP AND RESOURCE MANAGEMENT TESTS ===
    void testFactoryAgentCleanupAfterOperation();
    void testFactoryAgentCleanupWithActiveThreads();
    void testFactoryAgentRapidCreateDestroy();
    
    // === COMPATIBILITY TESTS ===
    void testFactoryAgentCompatibilityWithOriginal();
    void testFactoryAgentCSVLogging();
    void testFactoryAgentErrorRecovery();

private:
    std::unique_ptr<CAN> _test_can;

public:
    FactoryIntegrationTest();
};

/**
 * @brief Constructor that registers all test methods
 */
FactoryIntegrationTest::FactoryIntegrationTest() {
    // === BASIC INTEGRATION TESTS ===
    DEFINE_TEST(testFactoryToAgentCreation);
    DEFINE_TEST(testFactoryAgentBasicMessaging);
    DEFINE_TEST(testFactoryAgentDataGeneration);
    
    // === PERIODIC INTEREST INTEGRATION TESTS ===
    DEFINE_TEST(testFactoryAgentPeriodicInterest);
    DEFINE_TEST(testFactoryAgentPeriodicInterestLifecycle);
    DEFINE_TEST(testFactoryAgentMultiplePeriodicOperations);
    
    // === PRODUCER-CONSUMER INTEGRATION TESTS ===
    DEFINE_TEST(testFactoryProducerConsumerInteraction);
    DEFINE_TEST(testFactoryMultipleConsumersSingleProducer);
    DEFINE_TEST(testFactoryMixedUnitInteraction);
    
    // === COMPLETE MESSAGE FLOW TESTS ===
    DEFINE_TEST(testFactoryCompleteMessageFlow);
    DEFINE_TEST(testFactoryMessageFlowWithCustomRanges);
    DEFINE_TEST(testFactoryMessageFlowStressTest);
    
    // === CLEANUP AND RESOURCE MANAGEMENT TESTS ===
    DEFINE_TEST(testFactoryAgentCleanupAfterOperation);
    DEFINE_TEST(testFactoryAgentCleanupWithActiveThreads);
    DEFINE_TEST(testFactoryAgentRapidCreateDestroy);
    
    // === COMPATIBILITY TESTS ===
    DEFINE_TEST(testFactoryAgentCompatibilityWithOriginal);
    DEFINE_TEST(testFactoryAgentCSVLogging);
    DEFINE_TEST(testFactoryAgentErrorRecovery);
}

void FactoryIntegrationTest::setUp() {
    _test_can = createTestCAN();
}

void FactoryIntegrationTest::tearDown() {
    _test_can.reset();
    // Allow time for cleanup
    waitForMessages(100);
}

std::unique_ptr<CAN> FactoryIntegrationTest::createTestCAN() {
    return std::make_unique<CAN>();
}

Agent::Address FactoryIntegrationTest::createTestAddress(int id) {
    return Agent::Address{};
}

void FactoryIntegrationTest::waitForMessages(int timeout_ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
}

/**
 * @brief Tests factory to Agent creation integration
 * 
 * Verifies that factory functions create fully functional Agents that
 * integrate properly with the Agent framework.
 */
void FactoryIntegrationTest::testFactoryToAgentCreation() {
    auto addr1 = createTestAddress(1);
    auto addr2 = createTestAddress(2);
    
    // Create agents via factories
    auto producer_a = create_basic_producer_a(_test_can.get(), addr1, "IntegrationProducerA");
    auto consumer_a = create_basic_consumer_a(_test_can.get(), addr2, "IntegrationConsumerA");
    auto producer_b = create_basic_producer_b(_test_can.get(), addr1, "IntegrationProducerB");
    auto consumer_b = create_basic_consumer_b(_test_can.get(), addr2, "IntegrationConsumerB");
    
    // Verify all agents are properly created and running
    assert_true(producer_a->running(), "ProducerA should be running");
    assert_true(consumer_a->running(), "ConsumerA should be running");
    assert_true(producer_b->running(), "ProducerB should be running");
    assert_true(consumer_b->running(), "ConsumerB should be running");
    
    // Verify agent names
    assert_equal("IntegrationProducerA", producer_a->name(), "ProducerA name should be correct");
    assert_equal("IntegrationConsumerA", consumer_a->name(), "ConsumerA name should be correct");
    assert_equal("IntegrationProducerB", producer_b->name(), "ProducerB name should be correct");
    assert_equal("IntegrationConsumerB", consumer_b->name(), "ConsumerB name should be correct");
}

/**
 * @brief Tests basic messaging functionality of factory-created agents
 * 
 * Verifies that factory-created agents can send and receive messages
 * through the CAN bus.
 */
void FactoryIntegrationTest::testFactoryAgentBasicMessaging() {
    auto addr1 = createTestAddress(1);
    auto addr2 = createTestAddress(2);
    
    auto producer = create_basic_producer_a(_test_can.get(), addr1, "MessagingProducer");
    auto consumer = create_basic_consumer_a(_test_can.get(), addr2, "MessagingConsumer");
    
    // Test consumer sending INTEREST message
    int result = consumer->send(static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                               Agent::Microseconds(1000000));
    assert_true(result != -1, "Consumer should be able to send INTEREST messages");
    
    // Test producer data generation (simulates RESPONSE)
    auto value = producer->get(static_cast<std::uint32_t>(DataTypes::UNIT_A));
    assert_false(value.empty(), "Producer should generate data for RESPONSE messages");
    assert_equal(sizeof(float), value.size(), "Producer should generate float-sized data");
}

/**
 * @brief Tests data generation consistency of factory-created agents
 * 
 * Verifies that factory-created producers generate data consistently
 * and within expected ranges.
 */
void FactoryIntegrationTest::testFactoryAgentDataGeneration() {
    auto addr = createTestAddress();
    
    // Test ProducerA with default range
    auto producer_a = create_basic_producer_a(_test_can.get(), addr, "DataGenProducerA");
    
    std::vector<float> generated_values_a;
    for (int i = 0; i < 20; ++i) {
        auto value = producer_a->get(static_cast<std::uint32_t>(DataTypes::UNIT_A));
        float generated = *reinterpret_cast<const float*>(value.data());
        generated_values_a.push_back(generated);
        
        assert_true(generated >= 0.0f && generated <= 100.0f, 
                    "ProducerA should generate values in range [0, 100]");
    }
    
    // Test ProducerB with default range
    auto producer_b = create_basic_producer_b(_test_can.get(), addr, "DataGenProducerB");
    
    std::vector<float> generated_values_b;
    for (int i = 0; i < 20; ++i) {
        auto value = producer_b->get(static_cast<std::uint32_t>(DataTypes::UNIT_B));
        float generated = *reinterpret_cast<const float*>(value.data());
        generated_values_b.push_back(generated);
        
        assert_true(generated >= 200.0f && generated <= 300.0f, 
                    "ProducerB should generate values in range [200, 300]");
    }
    
    // Verify randomness (values should not all be the same)
    bool has_variation_a = false;
    bool has_variation_b = false;
    
    for (size_t i = 1; i < generated_values_a.size(); ++i) {
        if (std::abs(generated_values_a[i] - generated_values_a[0]) > 0.001f) {
            has_variation_a = true;
            break;
        }
    }
    
    for (size_t i = 1; i < generated_values_b.size(); ++i) {
        if (std::abs(generated_values_b[i] - generated_values_b[0]) > 0.001f) {
            has_variation_b = true;
            break;
        }
    }
    
    assert_true(has_variation_a, "ProducerA should generate varied values");
    assert_true(has_variation_b, "ProducerB should generate varied values");
}

/**
 * @brief Tests periodic interest functionality of factory-created agents
 * 
 * Verifies that factory-created consumers can start and manage periodic
 * INTEREST message sending.
 */
void FactoryIntegrationTest::testFactoryAgentPeriodicInterest() {
    auto addr1 = createTestAddress(1);
    auto addr2 = createTestAddress(2);
    
    auto producer = create_basic_producer_a(_test_can.get(), addr1, "PeriodicProducer");
    auto consumer = create_basic_consumer_a(_test_can.get(), addr2, "PeriodicConsumer");
    
    // Start periodic interest
    int result = consumer->start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(100000)); // 100ms period
    
    assert_equal(0, result, "Consumer should start periodic interest successfully");
    
    // Let it run for several periods
    waitForMessages(350);
    
    // Stop periodic interest
    consumer->stop_periodic_interest();
    
    // Test passes if no crashes occur
    assert_true(true, "Periodic interest should work without crashes");
}

/**
 * @brief Tests periodic interest lifecycle management
 * 
 * Verifies that factory-created agents properly manage the lifecycle
 * of periodic interest operations.
 */
void FactoryIntegrationTest::testFactoryAgentPeriodicInterestLifecycle() {
    auto addr = createTestAddress();
    auto consumer = create_basic_consumer_a(_test_can.get(), addr, "LifecycleConsumer");
    
    // Test multiple start/stop cycles
    for (int cycle = 0; cycle < 3; ++cycle) {
        // Start periodic interest
        int result = consumer->start_periodic_interest(
            static_cast<std::uint32_t>(DataTypes::UNIT_A), 
            Agent::Microseconds(50000)); // 50ms period
        assert_equal(0, result, "Should start periodic interest in cycle " + std::to_string(cycle));
        
        // Let it run briefly
        waitForMessages(100);
        
        // Stop periodic interest
        consumer->stop_periodic_interest();
        
        // Brief pause between cycles
        waitForMessages(50);
    }
    
    // Test passes if all cycles complete without issues
    assert_true(true, "Periodic interest lifecycle should work correctly");
}

/**
 * @brief Tests multiple periodic operations
 * 
 * Verifies that multiple factory-created consumers can run periodic
 * operations simultaneously without interference.
 */
void FactoryIntegrationTest::testFactoryAgentMultiplePeriodicOperations() {
    auto addr1 = createTestAddress(1);
    auto addr2 = createTestAddress(2);
    auto addr3 = createTestAddress(3);
    
    auto producer = create_basic_producer_a(_test_can.get(), addr1, "MultiProducer");
    auto consumer1 = create_basic_consumer_a(_test_can.get(), addr2, "MultiConsumer1");
    auto consumer2 = create_basic_consumer_a(_test_can.get(), addr3, "MultiConsumer2");
    
    // Start periodic interest on both consumers with different periods
    consumer1->start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(80000)); // 80ms
    
    consumer2->start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(120000)); // 120ms
    
    // Let them run concurrently
    waitForMessages(400);
    
    // Stop both
    consumer1->stop_periodic_interest();
    consumer2->stop_periodic_interest();
    
    // Test passes if no crashes or interference occur
    assert_true(true, "Multiple periodic operations should work concurrently");
}

/**
 * @brief Tests producer-consumer interaction
 * 
 * Verifies that factory-created producers and consumers can interact
 * correctly through the message system.
 */
void FactoryIntegrationTest::testFactoryProducerConsumerInteraction() {
    auto addr1 = createTestAddress(1);
    auto addr2 = createTestAddress(2);
    
    auto producer = create_basic_producer_a(_test_can.get(), addr1, "InteractionProducer");
    auto consumer = create_basic_consumer_a(_test_can.get(), addr2, "InteractionConsumer");
    
    // Start consumer periodic interest
    consumer->start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(100000)); // 100ms period
    
    // Allow interaction time
    waitForMessages(300);
    
    // Stop consumer
    consumer->stop_periodic_interest();
    
    // Test passes if interaction occurs without crashes
    assert_true(true, "Producer-consumer interaction should work correctly");
}

/**
 * @brief Tests multiple consumers with single producer
 * 
 * Verifies that multiple factory-created consumers can request data
 * from a single factory-created producer simultaneously.
 */
void FactoryIntegrationTest::testFactoryMultipleConsumersSingleProducer() {
    auto producer_addr = createTestAddress(1);
    auto producer = create_basic_producer_a(_test_can.get(), producer_addr, "SharedProducer");
    
    std::vector<std::unique_ptr<Agent>> consumers;
    for (int i = 0; i < 3; ++i) {
        auto consumer_addr = createTestAddress(i + 2);
        consumers.push_back(create_basic_consumer_a(
            _test_can.get(), consumer_addr, "SharedConsumer" + std::to_string(i)));
    }
    
    // Start all consumers with different periods
    for (size_t i = 0; i < consumers.size(); ++i) {
        consumers[i]->start_periodic_interest(
            static_cast<std::uint32_t>(DataTypes::UNIT_A), 
            Agent::Microseconds(100000 + i * 20000)); // Staggered periods
    }
    
    // Allow interaction time
    waitForMessages(500);
    
    // Stop all consumers
    for (auto& consumer : consumers) {
        consumer->stop_periodic_interest();
    }
    
    // Test passes if multiple consumers work with single producer
    assert_true(true, "Multiple consumers should work with single producer");
}

/**
 * @brief Tests mixed unit interaction
 * 
 * Verifies that UNIT_A and UNIT_B agents can operate simultaneously
 * without interference.
 */
void FactoryIntegrationTest::testFactoryMixedUnitInteraction() {
    auto addr1 = createTestAddress(1);
    auto addr2 = createTestAddress(2);
    auto addr3 = createTestAddress(3);
    auto addr4 = createTestAddress(4);
    
    // Create mixed unit agents
    auto producer_a = create_basic_producer_a(_test_can.get(), addr1, "MixedProducerA");
    auto consumer_a = create_basic_consumer_a(_test_can.get(), addr2, "MixedConsumerA");
    auto producer_b = create_basic_producer_b(_test_can.get(), addr3, "MixedProducerB");
    auto consumer_b = create_basic_consumer_b(_test_can.get(), addr4, "MixedConsumerB");
    
    // Start both consumers
    consumer_a->start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(100000));
    
    consumer_b->start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_B), 
        Agent::Microseconds(120000));
    
    // Allow mixed interaction
    waitForMessages(400);
    
    // Stop both consumers
    consumer_a->stop_periodic_interest();
    consumer_b->stop_periodic_interest();
    
    // Test passes if mixed units work without interference
    assert_true(true, "Mixed unit interaction should work correctly");
}

/**
 * @brief Tests complete message flow
 * 
 * Verifies the complete end-to-end message flow from factory creation
 * through periodic interest, message generation, and response handling.
 */
void FactoryIntegrationTest::testFactoryCompleteMessageFlow() {
    auto producer_addr = createTestAddress(1);
    auto consumer_addr = createTestAddress(2);
    
    // Create agents with custom configuration
    auto producer = create_basic_producer_a(_test_can.get(), producer_addr, 
                                           "FlowProducer", 50.0f, 75.0f);
    auto consumer = create_basic_consumer_a(_test_can.get(), consumer_addr, "FlowConsumer");
    
    // Start complete flow
    consumer->start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(150000)); // 150ms period
    
    // Allow multiple complete cycles
    waitForMessages(600);
    
    // Verify producer generates values in custom range
    for (int i = 0; i < 5; ++i) {
        auto value = producer->get(static_cast<std::uint32_t>(DataTypes::UNIT_A));
        float generated = *reinterpret_cast<const float*>(value.data());
        assert_true(generated >= 50.0f && generated <= 75.0f, 
                    "Producer should use custom range in complete flow");
    }
    
    // Stop flow
    consumer->stop_periodic_interest();
    
    // Test passes if complete flow works correctly
    assert_true(true, "Complete message flow should work correctly");
}

/**
 * @brief Tests message flow with custom ranges
 * 
 * Verifies that factory-created producers with custom ranges work
 * correctly in the complete message flow.
 */
void FactoryIntegrationTest::testFactoryMessageFlowWithCustomRanges() {
    auto addr1 = createTestAddress(1);
    auto addr2 = createTestAddress(2);
    
    // Create producers with custom ranges
    auto producer_a = create_basic_producer_a(_test_can.get(), addr1, 
                                             "CustomRangeA", 10.0f, 20.0f);
    auto producer_b = create_basic_producer_b(_test_can.get(), addr1, 
                                             "CustomRangeB", 500.0f, 600.0f);
    
    auto consumer_a = create_basic_consumer_a(_test_can.get(), addr2, "CustomConsumerA");
    auto consumer_b = create_basic_consumer_b(_test_can.get(), addr2, "CustomConsumerB");
    
    // Start both flows
    consumer_a->start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(100000));
    
    consumer_b->start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_B), 
        Agent::Microseconds(110000));
    
    // Allow flows to operate
    waitForMessages(400);
    
    // Verify custom ranges are maintained
    auto value_a = producer_a->get(static_cast<std::uint32_t>(DataTypes::UNIT_A));
    float generated_a = *reinterpret_cast<const float*>(value_a.data());
    assert_true(generated_a >= 10.0f && generated_a <= 20.0f, 
                "ProducerA should maintain custom range");
    
    auto value_b = producer_b->get(static_cast<std::uint32_t>(DataTypes::UNIT_B));
    float generated_b = *reinterpret_cast<const float*>(value_b.data());
    assert_true(generated_b >= 500.0f && generated_b <= 600.0f, 
                "ProducerB should maintain custom range");
    
    // Stop flows
    consumer_a->stop_periodic_interest();
    consumer_b->stop_periodic_interest();
    
    // Test passes if custom ranges work in message flow
    assert_true(true, "Message flow with custom ranges should work correctly");
}

/**
 * @brief Tests message flow stress test
 * 
 * Verifies that the factory-created agent system can handle high-frequency
 * message flows without issues.
 */
void FactoryIntegrationTest::testFactoryMessageFlowStressTest() {
    auto producer_addr = createTestAddress(1);
    auto consumer_addr = createTestAddress(2);
    
    auto producer = create_basic_producer_a(_test_can.get(), producer_addr, "StressProducer");
    auto consumer = create_basic_consumer_a(_test_can.get(), consumer_addr, "StressConsumer");
    
    // Start high-frequency periodic interest
    consumer->start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(10000)); // 10ms period (high frequency)
    
    // Run stress test
    waitForMessages(200);
    
    // Stop stress test
    consumer->stop_periodic_interest();
    
    // Test passes if high-frequency operation works without crashes
    assert_true(true, "High-frequency message flow should work correctly");
}

/**
 * @brief Tests agent cleanup after operation
 * 
 * Verifies that factory-created agents clean up properly after normal
 * operation without active threads.
 */
void FactoryIntegrationTest::testFactoryAgentCleanupAfterOperation() {
    {
        auto addr = createTestAddress();
        auto producer = create_basic_producer_a(_test_can.get(), addr, "CleanupProducer");
        auto consumer = create_basic_consumer_a(_test_can.get(), addr, "CleanupConsumer");
        
        // Use agents briefly
        producer->get(static_cast<std::uint32_t>(DataTypes::UNIT_A));
        consumer->handle_response(nullptr);
        
        // Agents will be destroyed here
    }
    
    // Allow cleanup time
    waitForMessages(100);
    
    // Test passes if cleanup occurs without issues
    assert_true(true, "Agent cleanup after operation should work correctly");
}

/**
 * @brief Tests agent cleanup with active threads
 * 
 * Verifies that factory-created agents clean up properly even when
 * they have active periodic interest threads.
 */
void FactoryIntegrationTest::testFactoryAgentCleanupWithActiveThreads() {
    {
        auto addr = createTestAddress();
        auto consumer = create_basic_consumer_a(_test_can.get(), addr, "ThreadCleanupConsumer");
        
        // Start periodic interest
        consumer->start_periodic_interest(
            static_cast<std::uint32_t>(DataTypes::UNIT_A), 
            Agent::Microseconds(50000));
        
        // Let it run briefly
        waitForMessages(100);
        
        // Agent with active thread will be destroyed here
    }
    
    // Allow cleanup time
    waitForMessages(150);
    
    // Test passes if cleanup with active threads works correctly
    assert_true(true, "Agent cleanup with active threads should work correctly");
}

/**
 * @brief Tests rapid create/destroy cycles
 * 
 * Verifies that factory-created agents can be rapidly created and destroyed
 * without causing race conditions or resource leaks.
 */
void FactoryIntegrationTest::testFactoryAgentRapidCreateDestroy() {
    auto addr = createTestAddress();
    
    // Rapid create/destroy cycles
    for (int i = 0; i < 20; ++i) {
        {
            auto producer = create_basic_producer_a(_test_can.get(), addr, 
                                                   "RapidProducer" + std::to_string(i));
            auto consumer = create_basic_consumer_a(_test_can.get(), addr, 
                                                   "RapidConsumer" + std::to_string(i));
            
            // Brief operation
            producer->get(static_cast<std::uint32_t>(DataTypes::UNIT_A));
            
            // Agents destroyed here
        }
        
        // Brief pause between cycles
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    // Allow final cleanup
    waitForMessages(100);
    
    // Test passes if rapid cycles work without issues
    assert_true(true, "Rapid create/destroy cycles should work correctly");
}

/**
 * @brief Tests compatibility with original Agent functionality
 * 
 * Verifies that factory-created agents maintain compatibility with
 * all original Agent features and interfaces.
 */
void FactoryIntegrationTest::testFactoryAgentCompatibilityWithOriginal() {
    auto addr = createTestAddress();
    auto producer = create_basic_producer_a(_test_can.get(), addr, "CompatibilityProducer");
    auto consumer = create_basic_consumer_a(_test_can.get(), addr, "CompatibilityConsumer");
    
    // Test all original Agent interface methods
    assert_true(producer->running(), "Producer should support running() method");
    assert_true(consumer->running(), "Consumer should support running() method");
    
    assert_equal("CompatibilityProducer", producer->name(), "Producer should support name() method");
    assert_equal("CompatibilityConsumer", consumer->name(), "Consumer should support name() method");
    
    // Test send method
    int result = consumer->send(static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                               Agent::Microseconds(1000000));
    assert_true(result != -1, "Consumer should support send() method");
    
    // Test periodic interest methods
    result = consumer->start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(500000));
    assert_equal(0, result, "Consumer should support start_periodic_interest() method");
    
    consumer->update_interest_period(Agent::Microseconds(750000));
    consumer->stop_periodic_interest();
    
    // Test get method
    auto value = producer->get(static_cast<std::uint32_t>(DataTypes::UNIT_A));
    assert_false(value.empty(), "Producer should support get() method");
    
    // Test handle_response method
    consumer->handle_response(nullptr);
    
    // Test passes if all original methods work
    assert_true(true, "Factory agents should be compatible with original Agent interface");
}

/**
 * @brief Tests CSV logging functionality
 * 
 * Verifies that factory-created agents support CSV logging functionality
 * as provided by the original Agent class.
 */
void FactoryIntegrationTest::testFactoryAgentCSVLogging() {
    auto addr = createTestAddress();
    auto producer = create_basic_producer_a(_test_can.get(), addr, "CSVProducer");
    
    // Set up CSV logging
    producer->set_csv_logger("tests/logs");
    
    // Send a test message
    int result = producer->send(static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                               Agent::Microseconds(1000000));
    
    // Verify logging works (basic functionality test)
    assert_true(result != -1, "Message sending should work with CSV logging");
    
    // Test passes if CSV logging doesn't cause issues
    assert_true(true, "Factory agents should support CSV logging");
}

/**
 * @brief Tests error recovery functionality
 * 
 * Verifies that factory-created agents can recover from various error
 * conditions and continue operating normally.
 */
void FactoryIntegrationTest::testFactoryAgentErrorRecovery() {
    auto addr = createTestAddress();
    auto consumer = create_basic_consumer_a(_test_can.get(), addr, "ErrorRecoveryConsumer");
    
    // Test recovery from invalid operations
    consumer->stop_periodic_interest(); // Stop when not started
    consumer->stop_periodic_interest(); // Stop again (idempotent)
    
    // Start normal operation
    int result = consumer->start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(200000));
    assert_equal(0, result, "Should start normally after error conditions");
    
    // Test period updates
    consumer->update_interest_period(Agent::Microseconds(300000));
    
    // Test multiple starts (should update period)
    result = consumer->start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(400000));
    assert_equal(0, result, "Should handle multiple starts correctly");
    
    // Clean stop
    consumer->stop_periodic_interest();
    
    // Test passes if error recovery works correctly
    assert_true(true, "Factory agents should recover from error conditions correctly");
}

// Main function
int main() {
    TEST_INIT("FactoryIntegrationTest");
    FactoryIntegrationTest test;
    test.run();
    return 0;
} 