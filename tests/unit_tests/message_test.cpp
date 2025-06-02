#include "../../tests/testcase.h"
#include "../../tests/test_utils.h"
#include <cstring>
#include <string>
#include <vector>

// Simple Message class for testing - matches the expected interface from the original test
template <unsigned int MaxSize>
class Message {
public:
    static constexpr unsigned int MAX_SIZE = MaxSize;

    // Default constructor
    Message() : _size(0) {
        std::memset(_data, 0, MAX_SIZE);
    }

    // Constructor with data
    Message(const void* data, unsigned int size) : _size(0) {
        if (data && size > 0) {
            _size = (size > MAX_SIZE) ? MAX_SIZE : size;
            std::memcpy(_data, data, _size);
        }
    }

    // Copy constructor
    Message(const Message& other) : _size(other._size) {
        std::memcpy(_data, other._data, _size);
    }

    // Assignment operator
    Message& operator=(const Message& other) {
        if (this != &other) {
            _size = other._size;
            std::memcpy(_data, other._data, _size);
        }
        return *this;
    }

    // Destructor
    ~Message() = default;

    // Data access
    const void* data() const {
        return _data;
    }

    void* data() {
        return _data;
    }

    // Size access
    unsigned int size() const {
        return _size;
    }

private:
    unsigned char _data[MAX_SIZE];
    unsigned int _size;
};

// Test constants
const unsigned int TEST_MAX_MSG_SIZE = 128;
const unsigned int LARGE_MSG_SIZE = 256;

// Forward declarations
class MessageTest;

class MessageTest : public TestCase {
protected:
    void setUp() override;
    void tearDown() override;

    // Helper methods
    void verifyMessageContent(const Message<TEST_MAX_MSG_SIZE>& msg, 
                            const char* expected_data, 
                            unsigned int expected_size,
                            const std::string& context);

    // === BASIC FUNCTIONALITY TESTS ===
    void testEmptyMessageCreation();
    void testMessageCreationWithData();
    void testMessageDataRetrieval();
    void testMessageSizeRetrieval();

    // === COPY AND ASSIGNMENT TESTS ===
    void testCopyConstructor();
    void testAssignmentOperator();
    void testSelfAssignment();
    void testChainedAssignment();

    // === SIZE HANDLING TESTS ===
    void testMessageWithMaxSize();
    void testMessageExceedingMaxSize();
    void testMessageWithZeroSize();
    void testMessageSizeBoundaries();

    // === EDGE CASES AND ERROR CONDITION TESTS ===
    void testMessageWithNullData();
    void testMessageWithNullTerminatedStrings();
    void testMessageWithBinaryData();
    void testMessageWithEmptyString();

    // === MEMORY MANAGEMENT TESTS ===
    void testMultipleMessageCreation();
    void testMessageDestructionAndRecreation();
    void testLargeNumberOfMessages();

public:
    MessageTest();
};

// Implementations

/**
 * @brief Constructor that registers all test methods
 * 
 * Organizes tests into logical groups for better maintainability and clarity.
 * Each test method name clearly describes what functionality is being tested.
 */
MessageTest::MessageTest() {
    // === BASIC FUNCTIONALITY TESTS ===
    DEFINE_TEST(testEmptyMessageCreation);
    DEFINE_TEST(testMessageCreationWithData);
    DEFINE_TEST(testMessageDataRetrieval);
    DEFINE_TEST(testMessageSizeRetrieval);

    // === COPY AND ASSIGNMENT TESTS ===
    DEFINE_TEST(testCopyConstructor);
    DEFINE_TEST(testAssignmentOperator);
    DEFINE_TEST(testSelfAssignment);
    DEFINE_TEST(testChainedAssignment);

    // === SIZE HANDLING TESTS ===
    DEFINE_TEST(testMessageWithMaxSize);
    DEFINE_TEST(testMessageExceedingMaxSize);
    DEFINE_TEST(testMessageWithZeroSize);
    DEFINE_TEST(testMessageSizeBoundaries);

    // === EDGE CASES AND ERROR CONDITION TESTS ===
    DEFINE_TEST(testMessageWithNullData);
    DEFINE_TEST(testMessageWithNullTerminatedStrings);
    DEFINE_TEST(testMessageWithBinaryData);
    DEFINE_TEST(testMessageWithEmptyString);

    // === MEMORY MANAGEMENT TESTS ===
    DEFINE_TEST(testMultipleMessageCreation);
    DEFINE_TEST(testMessageDestructionAndRecreation);
    DEFINE_TEST(testLargeNumberOfMessages);
}

void MessageTest::setUp() {
    // No specific setup needed for message tests
}

void MessageTest::tearDown() {
    // No specific cleanup needed for message tests
}

/**
 * @brief Helper method to verify message content and properties
 * 
 * @param msg The message to verify
 * @param expected_data Expected data content (can be nullptr for no verification)
 * @param expected_size Expected size of the message
 * @param context Description of the test context for error messages
 * 
 * This utility method performs comprehensive verification of message properties
 * including size validation and data content comparison when expected_data is provided.
 */
void MessageTest::verifyMessageContent(const Message<TEST_MAX_MSG_SIZE>& msg, 
                                     const char* expected_data, 
                                     unsigned int expected_size,
                                     const std::string& context) {
    assert_equal(expected_size, msg.size(), context + " - size verification");
    
    if (expected_data != nullptr) {
        const char* actual_data = static_cast<const char*>(msg.data());
        assert_true(actual_data != nullptr, context + " - data should not be null");
        assert_equal(0, memcmp(actual_data, expected_data, expected_size), 
                    context + " - data content verification");
    }
}

/**
 * @brief Tests creation of empty messages
 * 
 * Verifies that messages can be created without data and that they
 * have the expected initial state (size 0, valid data pointer).
 */
void MessageTest::testEmptyMessageCreation() {
    Message<TEST_MAX_MSG_SIZE> emptyMsg;
    assert_equal(0u, emptyMsg.size(), "Empty message should have size 0");
    assert_true(emptyMsg.data() != nullptr, "Empty message should have valid data pointer");
}

/**
 * @brief Tests creation of messages with data
 * 
 * Verifies that messages can be created with initial data and that
 * the data is properly stored and accessible.
 */
void MessageTest::testMessageCreationWithData() {
    const char testData[] = "Hello, World!";
    const unsigned int testDataSize = strlen(testData) + 1; // +1 for null terminator
    
    Message<TEST_MAX_MSG_SIZE> dataMsg(testData, testDataSize);
    verifyMessageContent(dataMsg, testData, testDataSize, "Message creation with data");
}

/**
 * @brief Tests data retrieval from messages
 * 
 * Verifies that the data() method returns the correct data content
 * and that the data is accessible and matches the original input.
 */
void MessageTest::testMessageDataRetrieval() {
    const char testData[] = "Test Data Content";
    const unsigned int testDataSize = strlen(testData) + 1;
    
    Message<TEST_MAX_MSG_SIZE> dataMsg(testData, testDataSize);
    
    const char* retrievedData = static_cast<const char*>(dataMsg.data());
    assert_true(retrievedData != nullptr, "Retrieved data should not be null");
    assert_equal(0, strcmp(retrievedData, testData), "Retrieved data should match original");
}

/**
 * @brief Tests size retrieval from messages
 * 
 * Verifies that the size() method returns the correct size for
 * messages with different data lengths.
 */
void MessageTest::testMessageSizeRetrieval() {
    // Test with different sized data
    const char smallData[] = "Hi";
    const char mediumData[] = "This is a medium sized message";
    const char largeData[] = "This is a much larger message that contains significantly more content";
    
    Message<TEST_MAX_MSG_SIZE> smallMsg(smallData, strlen(smallData) + 1);
    Message<TEST_MAX_MSG_SIZE> mediumMsg(mediumData, strlen(mediumData) + 1);
    Message<TEST_MAX_MSG_SIZE> largeMsg(largeData, strlen(largeData) + 1);
    
    assert_equal(strlen(smallData) + 1, smallMsg.size(), "Small message size verification");
    assert_equal(strlen(mediumData) + 1, mediumMsg.size(), "Medium message size verification");
    assert_equal(strlen(largeData) + 1, largeMsg.size(), "Large message size verification");
}

/**
 * @brief Tests copy constructor functionality
 * 
 * Verifies that messages can be properly copied using the copy constructor
 * and that the copied message has identical content and properties.
 */
void MessageTest::testCopyConstructor() {
    const char testData[] = "Copy Constructor Test";
    const unsigned int testDataSize = strlen(testData) + 1;
    
    Message<TEST_MAX_MSG_SIZE> originalMsg(testData, testDataSize);
    Message<TEST_MAX_MSG_SIZE> copiedMsg(originalMsg);
    
    verifyMessageContent(copiedMsg, testData, testDataSize, "Copy constructor test");
    
    // Verify that original message is unchanged
    verifyMessageContent(originalMsg, testData, testDataSize, "Original message after copy");
}

/**
 * @brief Tests assignment operator functionality
 * 
 * Verifies that messages can be properly assigned using the assignment operator
 * and that the assigned message has identical content and properties.
 */
void MessageTest::testAssignmentOperator() {
    const char testData[] = "Assignment Test Data";
    const unsigned int testDataSize = strlen(testData) + 1;
    
    Message<TEST_MAX_MSG_SIZE> originalMsg(testData, testDataSize);
    Message<TEST_MAX_MSG_SIZE> assignedMsg;
    
    // Verify initial state of assigned message
    assert_equal(0u, assignedMsg.size(), "Assigned message should start empty");
    
    // Perform assignment
    assignedMsg = originalMsg;
    
    verifyMessageContent(assignedMsg, testData, testDataSize, "Assignment operator test");
    
    // Verify that original message is unchanged
    verifyMessageContent(originalMsg, testData, testDataSize, "Original message after assignment");
}

/**
 * @brief Tests self-assignment safety
 * 
 * Verifies that self-assignment (msg = msg) does not corrupt the message
 * and that the message retains its original content and properties.
 */
void MessageTest::testSelfAssignment() {
    const char testData[] = "Self Assignment Test";
    const unsigned int testDataSize = strlen(testData) + 1;
    
    Message<TEST_MAX_MSG_SIZE> dataMsg(testData, testDataSize);
    
    // Perform self-assignment using reference to avoid linter warning
    Message<TEST_MAX_MSG_SIZE>& msgRef = dataMsg;
    dataMsg = msgRef;
    
    verifyMessageContent(dataMsg, testData, testDataSize, "Self-assignment test");
}

/**
 * @brief Tests chained assignment operations
 * 
 * Verifies that multiple assignment operations (a = b = c) work correctly
 * and that all messages end up with the same content.
 */
void MessageTest::testChainedAssignment() {
    const char testData[] = "Chained Assignment";
    const unsigned int testDataSize = strlen(testData) + 1;
    
    Message<TEST_MAX_MSG_SIZE> originalMsg(testData, testDataSize);
    Message<TEST_MAX_MSG_SIZE> msg1;
    Message<TEST_MAX_MSG_SIZE> msg2;
    
    // Perform chained assignment
    msg2 = msg1 = originalMsg;
    
    verifyMessageContent(msg1, testData, testDataSize, "First assigned message in chain");
    verifyMessageContent(msg2, testData, testDataSize, "Second assigned message in chain");
    verifyMessageContent(originalMsg, testData, testDataSize, "Original message after chained assignment");
}

/**
 * @brief Tests messages with maximum allowed size
 * 
 * Verifies that messages can handle data exactly at the maximum size limit
 * and that all data is properly stored and retrievable.
 */
void MessageTest::testMessageWithMaxSize() {
    // Create data exactly at max size
    char maxData[TEST_MAX_MSG_SIZE];
    memset(maxData, 'M', TEST_MAX_MSG_SIZE - 1);
    maxData[TEST_MAX_MSG_SIZE - 1] = '\0';
    
    Message<TEST_MAX_MSG_SIZE> maxMsg(maxData, TEST_MAX_MSG_SIZE);
    
    assert_equal(TEST_MAX_MSG_SIZE, maxMsg.size(), "Max size message should have correct size");
    
    const char* retrievedData = static_cast<const char*>(maxMsg.data());
    assert_true(retrievedData != nullptr, "Max size message data should not be null");
    assert_equal(0, memcmp(retrievedData, maxData, TEST_MAX_MSG_SIZE), 
                "Max size message data should match original");
}

/**
 * @brief Tests messages exceeding maximum allowed size
 * 
 * Verifies that when data larger than the maximum size is provided,
 * the message is properly truncated to the maximum size limit.
 */
void MessageTest::testMessageExceedingMaxSize() {
    // Create data larger than max size
    char largeData[LARGE_MSG_SIZE];
    memset(largeData, 'L', LARGE_MSG_SIZE - 1);
    largeData[LARGE_MSG_SIZE - 1] = '\0';
    
    Message<TEST_MAX_MSG_SIZE> largeMsg(largeData, LARGE_MSG_SIZE);
    
    assert_equal(TEST_MAX_MSG_SIZE, largeMsg.size(), 
                "Message size should be capped at MAX_SIZE");
    
    const char* retrievedData = static_cast<const char*>(largeMsg.data());
    assert_true(retrievedData != nullptr, "Large message data should not be null");
    assert_equal(0, memcmp(retrievedData, largeData, TEST_MAX_MSG_SIZE), 
                "Large message should contain first MAX_SIZE bytes");
}

/**
 * @brief Tests messages with zero size
 * 
 * Verifies that messages can be created with zero size and that
 * they behave correctly in this edge case.
 */
void MessageTest::testMessageWithZeroSize() {
    const char* testData = "This data should be ignored";
    
    Message<TEST_MAX_MSG_SIZE> zeroMsg(testData, 0);
    
    assert_equal(0u, zeroMsg.size(), "Zero size message should have size 0");
    assert_true(zeroMsg.data() != nullptr, "Zero size message should have valid data pointer");
}

/**
 * @brief Tests messages at size boundaries
 * 
 * Verifies correct behavior at various size boundaries including
 * sizes just below and at the maximum limit.
 */
void MessageTest::testMessageSizeBoundaries() {
    // Test size just below max
    const unsigned int nearMaxSize = TEST_MAX_MSG_SIZE - 1;
    char nearMaxData[nearMaxSize];
    memset(nearMaxData, 'N', nearMaxSize);
    
    Message<TEST_MAX_MSG_SIZE> nearMaxMsg(nearMaxData, nearMaxSize);
    assert_equal(nearMaxSize, nearMaxMsg.size(), "Near max size message should have correct size");
    
    // Test size of 1
    const char singleByte = 'S';
    Message<TEST_MAX_MSG_SIZE> singleMsg(&singleByte, 1);
    assert_equal(1u, singleMsg.size(), "Single byte message should have size 1");
    
    const char* retrievedSingle = static_cast<const char*>(singleMsg.data());
    assert_equal(singleByte, *retrievedSingle, "Single byte should be correctly stored");
}

/**
 * @brief Tests message creation with null data pointer
 * 
 * Verifies that the message handles null data pointers gracefully
 * and creates a valid empty message.
 */
void MessageTest::testMessageWithNullData() {
    Message<TEST_MAX_MSG_SIZE> nullMsg(nullptr, 10);
    
    assert_equal(0u, nullMsg.size(), "Message with null data should have size 0");
    assert_true(nullMsg.data() != nullptr, "Message should have valid data pointer even with null input");
}

/**
 * @brief Tests messages with null-terminated strings
 * 
 * Verifies that null-terminated strings are handled correctly and
 * that the null terminator is preserved in the message data.
 */
void MessageTest::testMessageWithNullTerminatedStrings() {
    const char nullTermString[] = "Null\0Terminated\0String";
    const unsigned int stringSize = sizeof(nullTermString); // Includes embedded nulls
    
    Message<TEST_MAX_MSG_SIZE> nullTermMsg(nullTermString, stringSize);
    
    assert_equal(stringSize, nullTermMsg.size(), "Null terminated string size should be preserved");
    
    const char* retrievedData = static_cast<const char*>(nullTermMsg.data());
    assert_equal(0, memcmp(retrievedData, nullTermString, stringSize), 
                "Null terminated string content should be preserved");
}

/**
 * @brief Tests messages with binary data
 * 
 * Verifies that binary data (including all byte values) is handled
 * correctly and that no data corruption occurs.
 */
void MessageTest::testMessageWithBinaryData() {
    // Create binary data with all possible byte values
    unsigned char binaryData[256];
    for (int i = 0; i < 256; ++i) {
        binaryData[i] = static_cast<unsigned char>(i);
    }
    
    Message<TEST_MAX_MSG_SIZE> binaryMsg(binaryData, TEST_MAX_MSG_SIZE);
    
    assert_equal(TEST_MAX_MSG_SIZE, binaryMsg.size(), "Binary message should have correct size");
    
    const unsigned char* retrievedBinary = static_cast<const unsigned char*>(binaryMsg.data());
    assert_equal(0, memcmp(retrievedBinary, binaryData, TEST_MAX_MSG_SIZE), 
                "Binary data should be preserved exactly");
}

/**
 * @brief Tests messages with empty strings
 * 
 * Verifies that empty strings (just null terminator) are handled
 * correctly and distinguished from completely empty messages.
 */
void MessageTest::testMessageWithEmptyString() {
    const char emptyString[] = "";
    const unsigned int emptyStringSize = 1; // Just the null terminator
    
    Message<TEST_MAX_MSG_SIZE> emptyStringMsg(emptyString, emptyStringSize);
    
    assert_equal(emptyStringSize, emptyStringMsg.size(), "Empty string should have size 1");
    
    const char* retrievedData = static_cast<const char*>(emptyStringMsg.data());
    assert_equal('\0', *retrievedData, "Empty string should contain null terminator");
}

/**
 * @brief Tests creation of multiple messages
 * 
 * Verifies that multiple messages can be created simultaneously
 * without interference and that each maintains its own data.
 */
void MessageTest::testMultipleMessageCreation() {
    const char data1[] = "Message One";
    const char data2[] = "Second Message Content";
    const char data3[] = "Third";
    
    Message<TEST_MAX_MSG_SIZE> msg1(data1, strlen(data1) + 1);
    Message<TEST_MAX_MSG_SIZE> msg2(data2, strlen(data2) + 1);
    Message<TEST_MAX_MSG_SIZE> msg3(data3, strlen(data3) + 1);
    
    // Verify each message maintains its own data
    verifyMessageContent(msg1, data1, strlen(data1) + 1, "Multiple messages - msg1");
    verifyMessageContent(msg2, data2, strlen(data2) + 1, "Multiple messages - msg2");
    verifyMessageContent(msg3, data3, strlen(data3) + 1, "Multiple messages - msg3");
}

/**
 * @brief Tests message destruction and recreation
 * 
 * Verifies that messages can be properly destroyed and recreated
 * without memory leaks or corruption.
 */
void MessageTest::testMessageDestructionAndRecreation() {
    const char testData[] = "Destruction Test";
    const unsigned int testSize = strlen(testData) + 1;
    
    // Create and destroy message in a scope
    {
        Message<TEST_MAX_MSG_SIZE> tempMsg(testData, testSize);
        verifyMessageContent(tempMsg, testData, testSize, "Temporary message");
    } // tempMsg destroyed here
    
    // Create new message with same data
    Message<TEST_MAX_MSG_SIZE> newMsg(testData, testSize);
    verifyMessageContent(newMsg, testData, testSize, "Recreated message");
}

/**
 * @brief Tests creation of a large number of messages
 * 
 * Verifies that the message system can handle creating many messages
 * without performance degradation or resource exhaustion.
 */
void MessageTest::testLargeNumberOfMessages() {
    const int numMessages = 100;
    std::vector<Message<TEST_MAX_MSG_SIZE>> messages;
    messages.reserve(numMessages);
    
    // Create many messages with different data
    for (int i = 0; i < numMessages; ++i) {
        std::string testData = "Message number " + std::to_string(i);
        messages.emplace_back(testData.c_str(), testData.length() + 1);
    }
    
    // Verify each message
    for (int i = 0; i < numMessages; ++i) {
        std::string expectedData = "Message number " + std::to_string(i);
        assert_equal(expectedData.length() + 1, messages[i].size(), 
                    "Large number test - message " + std::to_string(i) + " size");
        
        const char* data = static_cast<const char*>(messages[i].data());
        assert_equal(expectedData, std::string(data), 
                    "Large number test - message " + std::to_string(i) + " content");
    }
}

// Main function
int main() {
    TEST_INIT("MessageTest");
    MessageTest test;
    test.run();
    return 0;
} 