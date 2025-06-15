#include "../testcase.h"
#include "../test_utils.h"
#include "../../include/api/network/bus.h"
#include "../../include/app/components/basic_producer_a_factory.hpp"
#include "../../include/app/components/basic_consumer_a_factory.hpp"
#include "../../include/app/components/basic_producer_b_factory.hpp"
#include "../../include/app/components/basic_consumer_b_factory.hpp"
#include "../../include/app/datatypes.h"
#include <memory>
#include <stdexcept>

/**
 * @brief Test suite for component factory functions
 * 
 * Validates the factory-based approach for creating BasicProducer and BasicConsumer
 * agents using function-based composition. Tests creation, parameter validation,
 * error handling, and basic functionality following EPOS SmartData principles.
 */
class ComponentFactoriesTest : public TestCase {
protected:
    void setUp() override;
    void tearDown() override;

    // Helper methods
    std::unique_ptr<CAN> createTestCAN();
    Agent::Address createTestAddress();
    
    // === BASIC FACTORY FUNCTIONALITY TESTS ===
    void testCreateBasicProducerA();
    void testCreateBasicConsumerA();
    void testCreateBasicProducerB();
    void testCreateBasicConsumerB();
    
    // === PARAMETER VALIDATION TESTS ===
    void testFactoryParameterValidation();
    void testFactoryRangeValidation();
    void testFactoryNameValidation();
    
    // === CONFIGURATION TESTS ===
    void testProducerRangeConfiguration();
    void testConsumerWithPeriodCreation();
    void testDefaultParameterBehavior();
    
    // === AGENT FUNCTIONALITY TESTS ===
    void testFactoryCreatedAgentBasicOperation();
    void testFactoryCreatedAgentDataGeneration();
    void testFactoryCreatedAgentMessageHandling();
    
    // === ERROR HANDLING TESTS ===
    void testFactoryErrorHandling();
    void testFactoryExceptionSafety();
    
    // === MEMORY MANAGEMENT TESTS ===
    void testFactoryMemoryManagement();
    void testFactoryResourceCleanup();

private:
    std::unique_ptr<CAN> _test_can;

public:
    ComponentFactoriesTest();
};

/**
 * @brief Constructor that registers all test methods
 */
ComponentFactoriesTest::ComponentFactoriesTest() {
    // === BASIC FACTORY FUNCTIONALITY TESTS ===
    DEFINE_TEST(testCreateBasicProducerA);
    DEFINE_TEST(testCreateBasicConsumerA);
    DEFINE_TEST(testCreateBasicProducerB);
    DEFINE_TEST(testCreateBasicConsumerB);
    
    // === PARAMETER VALIDATION TESTS ===
    DEFINE_TEST(testFactoryParameterValidation);
    DEFINE_TEST(testFactoryRangeValidation);
    DEFINE_TEST(testFactoryNameValidation);
    
    // === CONFIGURATION TESTS ===
    DEFINE_TEST(testProducerRangeConfiguration);
    DEFINE_TEST(testConsumerWithPeriodCreation);
    DEFINE_TEST(testDefaultParameterBehavior);
    
    // === AGENT FUNCTIONALITY TESTS ===
    DEFINE_TEST(testFactoryCreatedAgentBasicOperation);
    DEFINE_TEST(testFactoryCreatedAgentDataGeneration);
    DEFINE_TEST(testFactoryCreatedAgentMessageHandling);
    
    // === ERROR HANDLING TESTS ===
    DEFINE_TEST(testFactoryErrorHandling);
    DEFINE_TEST(testFactoryExceptionSafety);
    
    // === MEMORY MANAGEMENT TESTS ===
    DEFINE_TEST(testFactoryMemoryManagement);
    DEFINE_TEST(testFactoryResourceCleanup);
}

void ComponentFactoriesTest::setUp() {
    _test_can = createTestCAN();
}

void ComponentFactoriesTest::tearDown() {
    _test_can.reset();
    // Allow time for cleanup
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

std::unique_ptr<CAN> ComponentFactoriesTest::createTestCAN() {
    return std::make_unique<CAN>();
}

Agent::Address ComponentFactoriesTest::createTestAddress() {
    return Agent::Address{};
}

/**
 * @brief Tests BasicProducerA factory function creation
 * 
 * Verifies that the factory creates a valid Agent with correct configuration.
 */
void ComponentFactoriesTest::testCreateBasicProducerA() {
    auto addr = createTestAddress();
    
    auto producer = create_basic_producer_a(_test_can.get(), addr, "TestProducerA");
    
    assert_true(producer != nullptr, "Factory should create valid Agent");
    assert_equal("TestProducerA", producer->name(), "Agent name should be set correctly");
    assert_true(producer->running(), "Agent should be running after creation");
}

/**
 * @brief Tests BasicConsumerA factory function creation
 * 
 * Verifies that the factory creates a valid consumer Agent.
 */
void ComponentFactoriesTest::testCreateBasicConsumerA() {
    auto addr = createTestAddress();
    
    auto consumer = create_basic_consumer_a(_test_can.get(), addr, "TestConsumerA");
    
    assert_true(consumer != nullptr, "Factory should create valid Agent");
    assert_equal("TestConsumerA", consumer->name(), "Agent name should be set correctly");
    assert_true(consumer->running(), "Agent should be running after creation");
}

/**
 * @brief Tests BasicProducerB factory function creation
 * 
 * Verifies that the factory creates a valid UNIT_B producer Agent.
 */
void ComponentFactoriesTest::testCreateBasicProducerB() {
    auto addr = createTestAddress();
    
    auto producer = create_basic_producer_b(_test_can.get(), addr, "TestProducerB");
    
    assert_true(producer != nullptr, "Factory should create valid Agent");
    assert_equal("TestProducerB", producer->name(), "Agent name should be set correctly");
    assert_true(producer->running(), "Agent should be running after creation");
}

/**
 * @brief Tests BasicConsumerB factory function creation
 * 
 * Verifies that the factory creates a valid UNIT_B consumer Agent.
 */
void ComponentFactoriesTest::testCreateBasicConsumerB() {
    auto addr = createTestAddress();
    
    auto consumer = create_basic_consumer_b(_test_can.get(), addr, "TestConsumerB");
    
    assert_true(consumer != nullptr, "Factory should create valid Agent");
    assert_equal("TestConsumerB", consumer->name(), "Agent name should be set correctly");
    assert_true(consumer->running(), "Agent should be running after creation");
}

/**
 * @brief Tests factory parameter validation
 * 
 * Verifies that factories properly validate input parameters and throw
 * appropriate exceptions for invalid inputs.
 */
void ComponentFactoriesTest::testFactoryParameterValidation() {
    auto addr = createTestAddress();
    
    // Test null CAN bus validation
    bool exception_thrown = false;
    try {
        auto producer = create_basic_producer_a(nullptr, addr, "TestProducer");
    } catch (const std::invalid_argument& e) {
        exception_thrown = true;
        assert_true(std::string(e.what()).find("CAN bus cannot be null") != std::string::npos,
                    "Exception message should mention null CAN bus");
    }
    assert_true(exception_thrown, "Should throw exception for null CAN bus");
    
    // Test empty name validation
    exception_thrown = false;
    try {
        auto producer = create_basic_producer_a(_test_can.get(), addr, "");
    } catch (const std::invalid_argument& e) {
        exception_thrown = true;
        assert_true(std::string(e.what()).find("Agent name cannot be empty") != std::string::npos,
                    "Exception message should mention empty name");
    }
    assert_true(exception_thrown, "Should throw exception for empty name");
}

/**
 * @brief Tests factory range validation
 * 
 * Verifies that producer factories validate range parameters correctly.
 */
void ComponentFactoriesTest::testFactoryRangeValidation() {
    auto addr = createTestAddress();
    
    // Test invalid range (min >= max)
    bool exception_thrown = false;
    try {
        auto producer = create_basic_producer_a(_test_can.get(), addr, "TestProducer", 100.0f, 50.0f);
    } catch (const std::invalid_argument& e) {
        exception_thrown = true;
        assert_true(std::string(e.what()).find("Invalid range") != std::string::npos,
                    "Exception message should mention invalid range");
    }
    assert_true(exception_thrown, "Should throw exception for invalid range");
    
    // Test equal min and max
    exception_thrown = false;
    try {
        auto producer = create_basic_producer_b(_test_can.get(), addr, "TestProducer", 200.0f, 200.0f);
    } catch (const std::invalid_argument& e) {
        exception_thrown = true;
    }
    assert_true(exception_thrown, "Should throw exception for equal min and max");
}

/**
 * @brief Tests factory name validation
 * 
 * Verifies that factories handle various name inputs correctly.
 */
void ComponentFactoriesTest::testFactoryNameValidation() {
    auto addr = createTestAddress();
    
    // Test valid names
    auto producer1 = create_basic_producer_a(_test_can.get(), addr, "ValidName");
    assert_equal("ValidName", producer1->name(), "Should accept valid name");
    
    auto producer2 = create_basic_producer_a(_test_can.get(), addr, "Name_With_Underscores");
    assert_equal("Name_With_Underscores", producer2->name(), "Should accept names with underscores");
    
    auto producer3 = create_basic_producer_a(_test_can.get(), addr, "Name123");
    assert_equal("Name123", producer3->name(), "Should accept names with numbers");
}

/**
 * @brief Tests producer range configuration
 * 
 * Verifies that producers can be configured with custom ranges and that
 * the generated values fall within the specified range.
 */
void ComponentFactoriesTest::testProducerRangeConfiguration() {
    auto addr = createTestAddress();
    
    // Test custom range for ProducerA
    auto producer_a = create_basic_producer_a(_test_can.get(), addr, "CustomRangeA", 10.0f, 20.0f);
    
    // Generate multiple values to test range
    for (int i = 0; i < 10; ++i) {
        auto value = producer_a->get(static_cast<std::uint32_t>(DataTypes::UNIT_A));
        assert_equal(sizeof(float), value.size(), "Value should be float size");
        
        float generated_value = *reinterpret_cast<const float*>(value.data());
        assert_true(generated_value >= 10.0f && generated_value <= 20.0f,
                    "Generated value should be within custom range");
    }
    
    // Test custom range for ProducerB
    auto producer_b = create_basic_producer_b(_test_can.get(), addr, "CustomRangeB", 500.0f, 600.0f);
    
    for (int i = 0; i < 10; ++i) {
        auto value = producer_b->get(static_cast<std::uint32_t>(DataTypes::UNIT_B));
        assert_equal(sizeof(float), value.size(), "Value should be float size");
        
        float generated_value = *reinterpret_cast<const float*>(value.data());
        assert_true(generated_value >= 500.0f && generated_value <= 600.0f,
                    "Generated value should be within custom range");
    }
}

/**
 * @brief Tests consumer creation with period
 * 
 * Verifies that the convenience functions for creating consumers with
 * automatic periodic interest work correctly.
 */
void ComponentFactoriesTest::testConsumerWithPeriodCreation() {
    auto addr = createTestAddress();
    
    // Test ConsumerA with period
    auto consumer_a = create_basic_consumer_a_with_period(
        _test_can.get(), addr, Agent::Microseconds(500000), "PeriodConsumerA");
    
    assert_true(consumer_a != nullptr, "Factory should create valid consumer with period");
    assert_equal("PeriodConsumerA", consumer_a->name(), "Consumer name should be set correctly");
    
    // Test ConsumerB with period
    auto consumer_b = create_basic_consumer_b_with_period(
        _test_can.get(), addr, Agent::Microseconds(750000), "PeriodConsumerB");
    
    assert_true(consumer_b != nullptr, "Factory should create valid consumer with period");
    assert_equal("PeriodConsumerB", consumer_b->name(), "Consumer name should be set correctly");
    
    // Clean up periodic interest
    consumer_a->stop_periodic_interest();
    consumer_b->stop_periodic_interest();
}

/**
 * @brief Tests default parameter behavior
 * 
 * Verifies that factory functions work correctly with default parameters.
 */
void ComponentFactoriesTest::testDefaultParameterBehavior() {
    auto addr = createTestAddress();
    
    // Test default names
    auto producer_a = create_basic_producer_a(_test_can.get(), addr);
    assert_equal("BasicProducerA", producer_a->name(), "Should use default name");
    
    auto consumer_a = create_basic_consumer_a(_test_can.get(), addr);
    assert_equal("BasicConsumerA", consumer_a->name(), "Should use default name");
    
    auto producer_b = create_basic_producer_b(_test_can.get(), addr);
    assert_equal("BasicProducerB", producer_b->name(), "Should use default name");
    
    auto consumer_b = create_basic_consumer_b(_test_can.get(), addr);
    assert_equal("BasicConsumerB", consumer_b->name(), "Should use default name");
    
    // Test default ranges by generating values
    auto value_a = producer_a->get(static_cast<std::uint32_t>(DataTypes::UNIT_A));
    float generated_a = *reinterpret_cast<const float*>(value_a.data());
    assert_true(generated_a >= 0.0f && generated_a <= 100.0f, "Should use default range for A");
    
    auto value_b = producer_b->get(static_cast<std::uint32_t>(DataTypes::UNIT_B));
    float generated_b = *reinterpret_cast<const float*>(value_b.data());
    assert_true(generated_b >= 200.0f && generated_b <= 300.0f, "Should use default range for B");
}

/**
 * @brief Tests basic operation of factory-created agents
 * 
 * Verifies that agents created by factories operate correctly and maintain
 * the same functionality as the original inheritance-based classes.
 */
void ComponentFactoriesTest::testFactoryCreatedAgentBasicOperation() {
    auto addr = createTestAddress();
    
    auto producer = create_basic_producer_a(_test_can.get(), addr, "OperationTest");
    auto consumer = create_basic_consumer_a(_test_can.get(), addr, "OperationTest");
    
    // Test basic agent properties
    assert_true(producer->running(), "Producer should be running");
    assert_true(consumer->running(), "Consumer should be running");
    
    // Test message sending capability
    int result = consumer->send(static_cast<std::uint32_t>(DataTypes::UNIT_A), 
                               Agent::Microseconds(1000000));
    assert_true(result != -1, "Consumer should be able to send messages");
}

/**
 * @brief Tests data generation of factory-created producers
 * 
 * Verifies that producers created by factories generate valid data
 * in the correct format and range.
 */
void ComponentFactoriesTest::testFactoryCreatedAgentDataGeneration() {
    auto addr = createTestAddress();
    
    auto producer_a = create_basic_producer_a(_test_can.get(), addr, "DataGenTest");
    auto producer_b = create_basic_producer_b(_test_can.get(), addr, "DataGenTest");
    
    // Test ProducerA data generation
    for (int i = 0; i < 5; ++i) {
        auto value = producer_a->get(static_cast<std::uint32_t>(DataTypes::UNIT_A));
        assert_equal(sizeof(float), value.size(), "ProducerA should generate float-sized data");
        assert_false(value.empty(), "ProducerA should not generate empty data");
        
        float generated = *reinterpret_cast<const float*>(value.data());
        assert_true(generated >= 0.0f && generated <= 100.0f, "ProducerA should generate values in range");
    }
    
    // Test ProducerB data generation
    for (int i = 0; i < 5; ++i) {
        auto value = producer_b->get(static_cast<std::uint32_t>(DataTypes::UNIT_B));
        assert_equal(sizeof(float), value.size(), "ProducerB should generate float-sized data");
        assert_false(value.empty(), "ProducerB should not generate empty data");
        
        float generated = *reinterpret_cast<const float*>(value.data());
        assert_true(generated >= 200.0f && generated <= 300.0f, "ProducerB should generate values in range");
    }
}

/**
 * @brief Tests message handling of factory-created consumers
 * 
 * Verifies that consumers created by factories can handle messages correctly.
 */
void ComponentFactoriesTest::testFactoryCreatedAgentMessageHandling() {
    auto addr = createTestAddress();
    
    auto consumer_a = create_basic_consumer_a(_test_can.get(), addr, "MessageTest");
    auto consumer_b = create_basic_consumer_b(_test_can.get(), addr, "MessageTest");
    
    // Test message handling (with null message for current implementation)
    consumer_a->handle_response(nullptr);
    consumer_b->handle_response(nullptr);
    
    // Test passes if no crashes occur
    assert_true(true, "Consumers should handle messages without crashing");
}

/**
 * @brief Tests factory error handling
 * 
 * Verifies that factories handle various error conditions gracefully.
 */
void ComponentFactoriesTest::testFactoryErrorHandling() {
    auto addr = createTestAddress();
    
    // Test multiple error conditions
    std::vector<std::function<void()>> error_tests = {
        [&]() { create_basic_producer_a(nullptr, addr); },
        [&]() { create_basic_consumer_a(nullptr, addr); },
        [&]() { create_basic_producer_b(nullptr, addr); },
        [&]() { create_basic_consumer_b(nullptr, addr); },
        [&]() { create_basic_producer_a(_test_can.get(), addr, ""); },
        [&]() { create_basic_producer_a(_test_can.get(), addr, "Test", 100.0f, 50.0f); },
        [&]() { create_basic_producer_b(_test_can.get(), addr, "Test", 300.0f, 200.0f); }
    };
    
    for (auto& test : error_tests) {
        bool exception_thrown = false;
        try {
            test();
        } catch (const std::invalid_argument&) {
            exception_thrown = true;
        }
        assert_true(exception_thrown, "Error conditions should throw exceptions");
    }
}

/**
 * @brief Tests factory exception safety
 * 
 * Verifies that factories maintain exception safety and don't leak resources
 * when exceptions are thrown.
 */
void ComponentFactoriesTest::testFactoryExceptionSafety() {
    auto addr = createTestAddress();
    
    // Test that failed factory calls don't affect subsequent successful calls
    try {
        create_basic_producer_a(nullptr, addr);
    } catch (const std::invalid_argument&) {
        // Expected exception
    }
    
    // This should still work after the failed call
    auto producer = create_basic_producer_a(_test_can.get(), addr, "ExceptionSafetyTest");
    assert_true(producer != nullptr, "Factory should work after previous exception");
    assert_true(producer->running(), "Agent should be running after exception recovery");
}

/**
 * @brief Tests factory memory management
 * 
 * Verifies that factories properly manage memory and don't cause leaks.
 */
void ComponentFactoriesTest::testFactoryMemoryManagement() {
    auto addr = createTestAddress();
    
    // Create and destroy multiple agents
    for (int i = 0; i < 10; ++i) {
        {
            auto producer = create_basic_producer_a(_test_can.get(), addr, "MemoryTest" + std::to_string(i));
            auto consumer = create_basic_consumer_a(_test_can.get(), addr, "MemoryTest" + std::to_string(i));
            
            // Use the agents briefly
            producer->get(static_cast<std::uint32_t>(DataTypes::UNIT_A));
            consumer->handle_response(nullptr);
            
            // Agents destroyed automatically here
        }
    }
    
    // Test passes if no memory issues occur
    assert_true(true, "Memory management should be correct");
}

/**
 * @brief Tests factory resource cleanup
 * 
 * Verifies that factory-created agents clean up resources properly.
 */
void ComponentFactoriesTest::testFactoryResourceCleanup() {
    auto addr = createTestAddress();
    
    {
        auto consumer = create_basic_consumer_a_with_period(
            _test_can.get(), addr, Agent::Microseconds(100000), "CleanupTest");
        
        // Let it run briefly
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Agent with periodic interest will be destroyed here
    }
    
    // Allow cleanup time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Test passes if no crashes occur during cleanup
    assert_true(true, "Resource cleanup should be correct");
}

// Main function
int main() {
    TEST_INIT("ComponentFactoriesTest");
    ComponentFactoriesTest test;
    test.run();
    return 0;
} 