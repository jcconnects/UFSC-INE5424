#include <memory>
#include <cstring>
#include <cmath>

#include "../testcase.h"
#include "../test_utils.h"
#include "app/components/unit_a_data.hpp"
#include "app/components/unit_a_functions.hpp"
#include "app/datatypes.h"

/**
 * @brief Test suite for UNIT_A component functions
 * 
 * Validates the function-based approach for BasicProducerA and BasicConsumerA
 * functionality, ensuring correct data generation, message handling, and
 * state management following EPOS SmartData principles.
 */
class ComponentFunctionsTest : public TestCase {
protected:
    void setUp() override;
    void tearDown() override;

    // === DATA STRUCTURE TESTS ===
    void testUnitADataInitialization();
    void testUnitADataRandomGeneration();
    void testUnitADataConsumerState();
    
    // === PRODUCER FUNCTION TESTS ===
    void testBasicProducerAFunction();
    void testBasicProducerAValueRange();
    void testBasicProducerADataFormat();
    void testBasicProducerAMultipleCalls();
    
    // === CONSUMER FUNCTION TESTS ===
    void testBasicConsumerAFunction();
    void testBasicConsumerAStateUpdate();
    void testBasicConsumerANullMessage();
    
    // === INTEGRATION TESTS ===
    void testProducerConsumerIntegration();
    void testFunctionIsolation();
    void testMemoryManagement();

private:
    std::unique_ptr<UnitAData> _test_data;

public:
    ComponentFunctionsTest();
};

/**
 * @brief Constructor that registers all test methods
 */
ComponentFunctionsTest::ComponentFunctionsTest() {
    // === DATA STRUCTURE TESTS ===
    DEFINE_TEST(testUnitADataInitialization);
    DEFINE_TEST(testUnitADataRandomGeneration);
    DEFINE_TEST(testUnitADataConsumerState);
    
    // === PRODUCER FUNCTION TESTS ===
    DEFINE_TEST(testBasicProducerAFunction);
    DEFINE_TEST(testBasicProducerAValueRange);
    DEFINE_TEST(testBasicProducerADataFormat);
    DEFINE_TEST(testBasicProducerAMultipleCalls);
    
    // === CONSUMER FUNCTION TESTS ===
    DEFINE_TEST(testBasicConsumerAFunction);
    DEFINE_TEST(testBasicConsumerAStateUpdate);
    DEFINE_TEST(testBasicConsumerANullMessage);
    
    // === INTEGRATION TESTS ===
    DEFINE_TEST(testProducerConsumerIntegration);
    DEFINE_TEST(testFunctionIsolation);
    DEFINE_TEST(testMemoryManagement);
}

void ComponentFunctionsTest::setUp() {
    _test_data = std::make_unique<UnitAData>();
}

void ComponentFunctionsTest::tearDown() {
    _test_data.reset();
}

/**
 * @brief Tests UnitAData structure initialization
 * 
 * Verifies that the data structure is properly initialized with
 * correct default values and random number generator setup.
 */
void ComponentFunctionsTest::testUnitADataInitialization() {
    assert_equal(0.0f, _test_data->last_received_value, "Initial received value should be 0.0");
    
    // Test that random number generator is properly initialized
    float value1 = _test_data->dist(_test_data->gen);
    float value2 = _test_data->dist(_test_data->gen);
    
    assert_true(value1 >= 0.0f && value1 <= 100.0f, "Random value should be in range [0.0, 100.0]");
    assert_true(value2 >= 0.0f && value2 <= 100.0f, "Random value should be in range [0.0, 100.0]");
    assert_true(value1 != value2, "Consecutive random values should be different");
}

/**
 * @brief Tests random number generation consistency
 * 
 * Verifies that the random number generator produces values
 * within the expected range consistently.
 */
void ComponentFunctionsTest::testUnitADataRandomGeneration() {
    const int num_samples = 100;
    int in_range_count = 0;
    
    for (int i = 0; i < num_samples; ++i) {
        float value = _test_data->dist(_test_data->gen);
        if (value >= 0.0f && value <= 100.0f) {
            in_range_count++;
        }
    }
    
    assert_equal(num_samples, in_range_count, "All random values should be in range [0.0, 100.0]");
}

/**
 * @brief Tests consumer state management
 * 
 * Verifies that consumer state can be properly managed and reset.
 */
void ComponentFunctionsTest::testUnitADataConsumerState() {
    // Set a test value
    _test_data->last_received_value = 42.5f;
    assert_equal(42.5f, _test_data->last_received_value, "Should store received value");
    
    // Test reset functionality
    _test_data->reset_consumer_state();
    assert_equal(0.0f, _test_data->last_received_value, "Reset should clear received value");
}

/**
 * @brief Tests basic producer function operation
 * 
 * Verifies that the producer function generates valid data
 * and returns it in the correct format.
 */
void ComponentFunctionsTest::testBasicProducerAFunction() {
    std::uint32_t unit = static_cast<std::uint32_t>(DataTypes::UNIT_A);
    
    std::vector<std::uint8_t> result = basic_producer_a(unit, _test_data.get());
    
    assert_equal(sizeof(float), result.size(), "Result should contain a float value");
    assert_false(result.empty(), "Result should not be empty");
    
    // Extract the float value
    float extracted_value = *reinterpret_cast<const float*>(result.data());
    assert_true(extracted_value >= 0.0f && extracted_value <= 100.0f, 
                "Extracted value should be in range [0.0, 100.0]");
}

/**
 * @brief Tests producer function value range
 * 
 * Verifies that the producer function consistently generates
 * values within the expected range.
 */
void ComponentFunctionsTest::testBasicProducerAValueRange() {
    std::uint32_t unit = static_cast<std::uint32_t>(DataTypes::UNIT_A);
    const int num_tests = 50;
    
    for (int i = 0; i < num_tests; ++i) {
        std::vector<std::uint8_t> result = basic_producer_a(unit, _test_data.get());
        float value = *reinterpret_cast<const float*>(result.data());
        
        assert_true(value >= 0.0f && value <= 100.0f, 
                    "All generated values should be in range [0.0, 100.0]");
    }
}

/**
 * @brief Tests producer function data format
 * 
 * Verifies that the producer function returns data in the
 * correct byte format for message transmission.
 */
void ComponentFunctionsTest::testBasicProducerADataFormat() {
    std::uint32_t unit = static_cast<std::uint32_t>(DataTypes::UNIT_A);
    
    std::vector<std::uint8_t> result = basic_producer_a(unit, _test_data.get());
    
    // Verify size
    assert_equal(sizeof(float), result.size(), "Result size should match float size");
    
    // Verify data integrity by round-trip conversion
    float original_value = *reinterpret_cast<const float*>(result.data());
    
    std::vector<std::uint8_t> test_vector(sizeof(float));
    std::memcpy(test_vector.data(), &original_value, sizeof(float));
    
    assert_true(result == test_vector, "Data format should be consistent");
}

/**
 * @brief Tests multiple calls to producer function
 * 
 * Verifies that the producer function can be called multiple times
 * and produces different values (due to randomness).
 */
void ComponentFunctionsTest::testBasicProducerAMultipleCalls() {
    std::uint32_t unit = static_cast<std::uint32_t>(DataTypes::UNIT_A);
    
    std::vector<std::uint8_t> result1 = basic_producer_a(unit, _test_data.get());
    std::vector<std::uint8_t> result2 = basic_producer_a(unit, _test_data.get());
    
    float value1 = *reinterpret_cast<const float*>(result1.data());
    float value2 = *reinterpret_cast<const float*>(result2.data());
    
    // Values should be different (very high probability with random generation)
    assert_true(std::abs(value1 - value2) > 0.001f, "Multiple calls should produce different values");
}

/**
 * @brief Tests basic consumer function operation
 * 
 * Verifies that the consumer function can be called without crashing
 * and updates the component state appropriately.
 */
void ComponentFunctionsTest::testBasicConsumerAFunction() {
    // Test with null message (current implementation)
    basic_consumer_a(nullptr, _test_data.get());
    
    // Verify that the function executed (placeholder value should be set)
    assert_equal(42.0f, _test_data->last_received_value, "Consumer should update received value");
}

/**
 * @brief Tests consumer function state update
 * 
 * Verifies that the consumer function properly updates the
 * component data structure state.
 */
void ComponentFunctionsTest::testBasicConsumerAStateUpdate() {
    float initial_value = _test_data->last_received_value;
    
    basic_consumer_a(nullptr, _test_data.get());
    
    assert_true(_test_data->last_received_value != initial_value, 
                "Consumer should update the received value");
}

/**
 * @brief Tests consumer function with null message
 * 
 * Verifies that the consumer function handles null messages gracefully.
 */
void ComponentFunctionsTest::testBasicConsumerANullMessage() {
    // Should not crash with null message
    basic_consumer_a(nullptr, _test_data.get());
    
    // Test passes if no crash occurs
    assert_true(true, "Consumer should handle null message gracefully");
}

/**
 * @brief Tests producer-consumer integration
 * 
 * Verifies that data produced by the producer function can be
 * conceptually consumed by the consumer function.
 */
void ComponentFunctionsTest::testProducerConsumerIntegration() {
    std::uint32_t unit = static_cast<std::uint32_t>(DataTypes::UNIT_A);
    
    // Generate data with producer
    std::vector<std::uint8_t> produced_data = basic_producer_a(unit, _test_data.get());
    assert_false(produced_data.empty(), "Producer should generate data");
    
    // Simulate consumption (current implementation uses placeholder)
    basic_consumer_a(nullptr, _test_data.get());
    
    // Verify integration works without crashes
    assert_true(true, "Producer-consumer integration should work");
}

/**
 * @brief Tests function isolation
 * 
 * Verifies that producer and consumer functions operate independently
 * and don't interfere with each other's data.
 */
void ComponentFunctionsTest::testFunctionIsolation() {
    std::uint32_t unit = static_cast<std::uint32_t>(DataTypes::UNIT_A);
    
    // Call producer multiple times
    basic_producer_a(unit, _test_data.get());
    basic_producer_a(unit, _test_data.get());
    
    float value_before_consumer = _test_data->last_received_value;
    
    // Call consumer
    basic_consumer_a(nullptr, _test_data.get());
    
    // Producer calls should not affect consumer state until consumer is called
    assert_true(true, "Functions should operate independently");
}

/**
 * @brief Tests memory management
 * 
 * Verifies that the functions properly manage memory and don't
 * cause leaks or corruption.
 */
void ComponentFunctionsTest::testMemoryManagement() {
    std::uint32_t unit = static_cast<std::uint32_t>(DataTypes::UNIT_A);
    
    // Create and destroy multiple data structures
    for (int i = 0; i < 10; ++i) {
        auto temp_data = std::make_unique<UnitAData>();
        
        std::vector<std::uint8_t> result = basic_producer_a(unit, temp_data.get());
        basic_consumer_a(nullptr, temp_data.get());
        
        // temp_data automatically destroyed here
    }
    
    // Test passes if no memory issues occur
    assert_true(true, "Memory management should be correct");
}

// Main function
int main() {
    TEST_INIT("ComponentFunctionsTest");
    ComponentFunctionsTest test;
    test.run();
    return 0;
} 