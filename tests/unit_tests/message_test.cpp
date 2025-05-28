#include <iostream>
#include <cstring>
#include "test_utils.h"
#include "network/message.h"

int main() {
    TEST_INIT("message_test");
    
    // Define constants for testing
    const unsigned int MAX_MSG_SIZE = 128;
    
    // Test 1: Create empty message
    Message<MAX_MSG_SIZE> emptyMsg;
    TEST_ASSERT(emptyMsg.size() == 0, "Empty message should have size 0");
    
    // Test 2: Create message with data
    const char testData[] = "Hello, World!";
    const unsigned int testDataSize = strlen(testData) + 1; // +1 for null terminator
    
    Message<MAX_MSG_SIZE> dataMsg(testData, testDataSize);
    TEST_ASSERT(dataMsg.size() == testDataSize, "Message size should match data size");
    
    // Test 3: Check data content
    const char* retrievedData = static_cast<const char*>(dataMsg.data());
    TEST_ASSERT(retrievedData != nullptr, "Message data should not be null");
    TEST_ASSERT(strcmp(retrievedData, testData) == 0, "Message data should match original data");
    
    // Test 4: Copy constructor
    Message<MAX_MSG_SIZE> copiedMsg(dataMsg);
    TEST_ASSERT(copiedMsg.size() == dataMsg.size(), "Copied message size should match original");
    
    const char* copiedData = static_cast<const char*>(copiedMsg.data());
    TEST_ASSERT(copiedData != nullptr, "Copied message data should not be null");
    TEST_ASSERT(strcmp(copiedData, testData) == 0, "Copied message data should match original data");
    
    // Test 5: Assignment operator
    Message<MAX_MSG_SIZE> assignedMsg;
    assignedMsg = dataMsg;
    TEST_ASSERT(assignedMsg.size() == dataMsg.size(), "Assigned message size should match original");
    
    const char* assignedData = static_cast<const char*>(assignedMsg.data());
    TEST_ASSERT(assignedData != nullptr, "Assigned message data should not be null");
    TEST_ASSERT(strcmp(assignedData, testData) == 0, "Assigned message data should match original data");
    
    // Test 6: Message with size larger than MAX_SIZE
    char largeData[MAX_MSG_SIZE * 2];
    memset(largeData, 'X', MAX_MSG_SIZE * 2);
    largeData[MAX_MSG_SIZE * 2 - 1] = '\0'; // Null terminate
    
    Message<MAX_MSG_SIZE> largeMsg(largeData, MAX_MSG_SIZE * 2);
    TEST_ASSERT(largeMsg.size() == MAX_MSG_SIZE, "Message size should be capped at MAX_SIZE");
    
    // Test 7: Self-assignment
    dataMsg = dataMsg;
    TEST_ASSERT(dataMsg.size() == testDataSize, "Self-assignment should not change size");
    
    const char* selfAssignedData = static_cast<const char*>(dataMsg.data());
    TEST_ASSERT(strcmp(selfAssignedData, testData) == 0, "Self-assignment should not change data");
    
    std::cout << "Message test passed successfully!" << std::endl;
    return 0;
} 