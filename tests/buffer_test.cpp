#include <iostream>
#include "components/test_utils.h"
#include "buffer.h"

struct TestData {
    int value1;
    char value2;
    double value3;

    bool operator==(const TestData& other) const {
        return value1 == other.value1 && 
               value2 == other.value2 && 
               value3 == other.value3;
    }
};

int main() {
    TEST_INIT("buffer_test");

    // Test 1: Create empty buffer
    Buffer<TestData> buffer;
    TEST_ASSERT(buffer.size() == 0, "Empty buffer should have size 0");
    
    // Test 2: Set data and check size
    TestData testData = {42, 'A', 3.14};
    buffer.setData(&testData, sizeof(TestData));
    TEST_ASSERT(buffer.size() == sizeof(TestData), "Buffer size should match data size");
    
    // Test 3: Retrieve data and check content
    TestData* retrievedData = buffer.data();
    TEST_ASSERT(retrievedData != nullptr, "Retrieved data should not be null");
    TEST_ASSERT(*retrievedData == testData, "Retrieved data should match original data");
    
    // Test 4: Set data with larger size than MAX_SIZE
    char largeData[Buffer<TestData>::MAX_SIZE + 10];
    std::memset(largeData, 'X', Buffer<TestData>::MAX_SIZE + 10);
    buffer.setData(largeData, Buffer<TestData>::MAX_SIZE + 10);
    TEST_ASSERT(buffer.size() == Buffer<TestData>::MAX_SIZE, "Buffer size should be capped at MAX_SIZE");
    
    // Test 5: Clear buffer
    buffer.clear();
    TEST_ASSERT(buffer.size() == 0, "Buffer size should be 0 after clear");
    
    // Test 6: Data should be zeroed after clear
    bool allZero = true;
    uint8_t* dataPtr = reinterpret_cast<uint8_t*>(buffer.data());
    for (unsigned int i = 0; i < Buffer<TestData>::MAX_SIZE; i++) {
        if (dataPtr[i] != 0) {
            allZero = false;
            break;
        }
    }
    TEST_ASSERT(allZero, "Buffer data should be zeroed after clear");
    
    std::cout << "Buffer test passed successfully!" << std::endl;
    return 0;
} 