#include <iostream>
#include <string>
#include <cstdint>
#include <cassert>
#include <stdexcept>

#include "api/util/static_size_hashed_cache.h"

// Test struct mimicking the ValueCache structure from agent.h
struct TestValueCache {
    int unit;
    uint64_t timestamp;
    unsigned int size;
    
    TestValueCache() : unit(0), timestamp(0), size(0) {}
    TestValueCache(int u, uint64_t t, unsigned int s) : unit(u), timestamp(t), size(s) {}
    
    bool operator==(const TestValueCache& other) const {
        return unit == other.unit && timestamp == other.timestamp && size == other.size;
    }
};

// Array type similar to how ValueCache is used in agent.h
static const unsigned int UNITS_PER_VEHICLE = 5;
typedef TestValueCache TestValueCacheArray[UNITS_PER_VEHICLE];

/**
 * @brief Test cache creation and initial state
 * Verifies that newly created caches behave correctly for empty state
 */
void test_cache_creation() {
    std::cout << "[ RUN      ] test_cache_creation" << std::endl;
    
    StaticSizeHashedCache<TestValueCache, 10> cache;
    
    // Test that newly created cache doesn't contain any keys
    assert(!cache.contains(1));
    assert(!cache.contains(100));
    
    // Test that get returns nullptr for non-existent keys
    TestValueCache* result = cache.get(1);
    assert(result == nullptr);
    
    std::cout << "[       OK ] test_cache_creation" << std::endl;
}

/**
 * @brief Test adding and retrieving single values
 * Verifies basic add/get functionality with simple values
 */
void test_add_and_get_single_value() {
    std::cout << "[ RUN      ] test_add_and_get_single_value" << std::endl;
    
    StaticSizeHashedCache<TestValueCache, 10> cache;
    TestValueCache test_value(42, 1234567890ULL, 100);
    
    // Add a value to the cache
    cache.add(123, test_value);
    
    // Verify it was added
    assert(cache.contains(123));
    
    // Retrieve and verify the value
    TestValueCache* retrieved = cache.get(123);
    assert(retrieved != nullptr);
    assert(test_value.unit == retrieved->unit);
    assert(test_value.timestamp == retrieved->timestamp);
    assert(test_value.size == retrieved->size);
    
    std::cout << "[       OK ] test_add_and_get_single_value" << std::endl;
}

/**
 * @brief Test adding and retrieving array values (similar to agent.h usage)
 * Verifies functionality with array types as used in the actual codebase
 */
void test_add_and_get_array_values() {
    std::cout << "[ RUN      ] test_add_and_get_array_values" << std::endl;
    
    StaticSizeHashedCache<TestValueCache*, 5> cache;
    TestValueCacheArray test_array;
    
    // Initialize the array with test data
    for (unsigned int i = 0; i < UNITS_PER_VEHICLE; ++i) {
        test_array[i] = TestValueCache(i + 1, 1000000ULL + i, 50 + i);
    }
    
    // Add the array to the cache
    cache.add(456, test_array);
    
    // Verify it was added
    assert(cache.contains(456));
    
    // Retrieve and verify the array
    TestValueCache** retrieved = cache.get(456);
    assert(retrieved != nullptr);
    
    // Verify each element in the array
    for (unsigned int i = 0; i < UNITS_PER_VEHICLE; ++i) {
        assert(test_array[i].unit == (*retrieved)[i].unit);
        assert(test_array[i].timestamp == (*retrieved)[i].timestamp);
        assert(test_array[i].size == (*retrieved)[i].size);
    }
    
    std::cout << "[       OK ] test_add_and_get_array_values" << std::endl;
}

/**
 * @brief Test the contains method functionality
 * Verifies that contains correctly identifies present and absent keys
 */
void test_contains_method() {
    std::cout << "[ RUN      ] test_contains_method" << std::endl;
    
    StaticSizeHashedCache<TestValueCache, 10> cache;
    TestValueCache test_value(10, 9876543210ULL, 200);
    
    // Initially, key should not be present
    assert(!cache.contains(789));
    
    // Add the key-value pair
    cache.add(789, test_value);
    
    // Now key should be present
    assert(cache.contains(789));
    
    // Other keys should still not be present
    assert(!cache.contains(790));
    assert(!cache.contains(788));
    
    std::cout << "[       OK ] test_contains_method" << std::endl;
}

/**
 * @brief Test updating existing keys
 * Verifies that adding to an existing key updates its value
 */
void test_update_existing_key() {
    std::cout << "[ RUN      ] test_update_existing_key" << std::endl;
    
    StaticSizeHashedCache<TestValueCache, 10> cache;
    TestValueCache original_value(5, 1111111111ULL, 75);
    TestValueCache updated_value(15, 2222222222ULL, 150);
    
    // Add original value
    cache.add(555, original_value);
    
    // Verify original value
    TestValueCache* retrieved = cache.get(555);
    assert(original_value.unit == retrieved->unit);
    
    // Update with new value
    cache.add(555, updated_value);
    
    // Verify updated value
    retrieved = cache.get(555);
    assert(updated_value.unit == retrieved->unit);
    assert(updated_value.timestamp == retrieved->timestamp);
    assert(updated_value.size == retrieved->size);
    
    std::cout << "[       OK ] test_update_existing_key" << std::endl;
}

/**
 * @brief Test collision handling with linear probing
 * Verifies that hash collisions are handled correctly through linear probing
 */
void test_collision_handling() {
    std::cout << "[ RUN      ] test_collision_handling" << std::endl;
    
    StaticSizeHashedCache<TestValueCache, 10> cache;
    
    // Use keys that will likely cause hash collisions in a small cache
    long int key1 = 10;
    long int key2 = 20; // These should have same hash % 10 in a size-10 cache
    
    TestValueCache value1(1, 1000ULL, 10);
    TestValueCache value2(2, 2000ULL, 20);
    
    // Add both values
    cache.add(key1, value1);
    cache.add(key2, value2);
    
    // Both should be retrievable
    assert(cache.contains(key1));
    assert(cache.contains(key2));
    
    // Values should be correct
    TestValueCache* retrieved1 = cache.get(key1);
    TestValueCache* retrieved2 = cache.get(key2);
    
    assert(value1.unit == retrieved1->unit);
    assert(value2.unit == retrieved2->unit);
    
    std::cout << "[       OK ] test_collision_handling" << std::endl;
}

/**
 * @brief Test cache full exception
 * Verifies that the cache throws an exception when full and a new key is added
 */
void test_cache_full_exception() {
    std::cout << "[ RUN      ] test_cache_full_exception" << std::endl;
    
    StaticSizeHashedCache<TestValueCache, 3> small_cache; // Very small cache for testing
    TestValueCache test_value(1, 1000ULL, 10);
    
    // Fill the cache completely
    small_cache.add(1, test_value);
    small_cache.add(2, test_value);
    small_cache.add(3, test_value);
    
    // Adding a fourth different key should throw an exception
    bool exception_thrown = false;
    try {
        small_cache.add(4, test_value);
    } catch (const std::runtime_error&) {
        exception_thrown = true;
    }
    assert(exception_thrown);
    
    std::cout << "[       OK ] test_cache_full_exception" << std::endl;
}

/**
 * @brief Test getting non-existent keys
 * Verifies that getting non-existent keys returns nullptr
 */
void test_get_nonexistent_key() {
    std::cout << "[ RUN      ] test_get_nonexistent_key" << std::endl;
    
    StaticSizeHashedCache<TestValueCache, 10> cache;
    TestValueCache test_value(42, 5000ULL, 25);
    
    // Add one key
    cache.add(100, test_value);
    
    // Try to get different keys
    assert(cache.get(99) == nullptr);
    assert(cache.get(101) == nullptr);
    assert(cache.get(200) == nullptr);
    
    // But the existing key should work
    assert(cache.get(100) != nullptr);
    
    std::cout << "[       OK ] test_get_nonexistent_key" << std::endl;
}

/**
 * @brief Test multiple key operations
 * Verifies correct behavior with multiple keys and various operations
 */
void test_multiple_keys_operations() {
    std::cout << "[ RUN      ] test_multiple_keys_operations" << std::endl;
    
    StaticSizeHashedCache<TestValueCache, 10> cache;
    
    // Add multiple key-value pairs
    for (int i = 1; i <= 5; ++i) {
        TestValueCache value(i * 10, i * 1000ULL, i * 5);
        cache.add(i * 100, value);
    }
    
    // Verify all keys are present
    for (int i = 1; i <= 5; ++i) {
        assert(cache.contains(i * 100));
    }
    
    // Verify all values are correct
    for (int i = 1; i <= 5; ++i) {
        TestValueCache* retrieved = cache.get(i * 100);
        assert(retrieved != nullptr);
        assert(i * 10 == retrieved->unit);
        assert(static_cast<uint64_t>(i * 1000) == retrieved->timestamp);
        assert(static_cast<unsigned int>(i * 5) == retrieved->size);
    }
    
    // Update some values
    for (int i = 2; i <= 4; i += 2) {
        TestValueCache updated_value(i * 20, i * 2000ULL, i * 10);
        cache.add(i * 100, updated_value);
    }
    
    // Verify updates worked
    TestValueCache* retrieved = cache.get(200);
    assert(40 == retrieved->unit);
    
    retrieved = cache.get(400);
    assert(80 == retrieved->unit);
    
    std::cout << "[       OK ] test_multiple_keys_operations" << std::endl;
}

int main() {
    std::cout << "Running StaticSizeHashedCache tests..." << std::endl;
    
    try {
        test_cache_creation();
        test_add_and_get_single_value();
        test_add_and_get_array_values();
        test_contains_method();
        test_update_existing_key();
        test_collision_handling();
        test_cache_full_exception();
        test_get_nonexistent_key();
        test_multiple_keys_operations();
        
        std::cout << std::endl << "All tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cout << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "Test failed with unknown exception!" << std::endl;
        return 1;
    }
}