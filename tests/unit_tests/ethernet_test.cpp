#include <iostream>
#include <cstring>
#include <vector>

#include "../testcase.h"
#include "../test_utils.h"
#include "api/network/ethernet.h"

// Forward declarations and class interface
class EthernetTest;

class EthernetTest : public TestCase {
protected:
    void setUp() override;
    void tearDown() override;

    // Helper methods for creating test data
    Ethernet::Address createTestAddress(uint8_t byte0, uint8_t byte1, uint8_t byte2, 
                                      uint8_t byte3, uint8_t byte4, uint8_t byte5);
    Ethernet::Frame createTestFrame(const Ethernet::Address& dst, const Ethernet::Address& src, 
                                  uint16_t protocol);
    void fillFramePayload(Ethernet::Frame& frame, uint8_t pattern_start = 0);
    bool verifyFramePayload(const Ethernet::Frame& frame, uint8_t expected_pattern_start = 0);

    // === MAC ADDRESS TESTS ===
    void testMacAddressEquality();
    void testMacAddressInequality();
    void testMacAddressComparisonWithSameValues();
    void testMacAddressComparisonWithDifferentValues();

    // === NULL ADDRESS TESTS ===
    void testNullAddressIsAllZeros();
    void testNullAddressComparisonWithZeroAddress();
    void testNullAddressComparisonWithNonZeroAddress();

    // === MAC TO STRING CONVERSION TESTS ===
    void testMacToStringConversionBasicFunctionality();
    void testMacToStringConversionWithLowercaseHex();
    void testMacToStringConversionWithUppercaseHex();
    void testMacToStringConversionWithMixedValues();
    void testMacToStringConversionWithAllZeros();
    void testMacToStringConversionWithAllOnes();

    // === FRAME STRUCTURE TESTS ===
    void testFrameStructureSize();
    void testFrameHeaderSize();
    void testFrameMtuValue();
    void testFrameFieldAlignment();

    // === FRAME CREATION AND VALIDATION TESTS ===
    void testFrameCreationWithValidParameters();
    void testFrameDestinationAssignment();
    void testFrameSourceAssignment();
    void testFrameProtocolAssignment();
    void testFramePayloadManipulation();
    void testFramePayloadPatternValidation();

    // === EDGE CASES AND ROBUSTNESS TESTS ===
    void testMacAddressWithEdgeValues();
    void testFrameWithMaximumPayloadSize();
    void testFrameWithDifferentProtocolValues();

public:
    EthernetTest();
};

// Implementations

/**
 * @brief Constructor that registers all test methods
 * 
 * Organizes tests into logical groups for better maintainability and clarity.
 * Each test method name clearly describes what Ethernet functionality is being tested.
 */
EthernetTest::EthernetTest() {
    // === MAC ADDRESS TESTS ===
    DEFINE_TEST(testMacAddressEquality);
    DEFINE_TEST(testMacAddressInequality);
    DEFINE_TEST(testMacAddressComparisonWithSameValues);
    DEFINE_TEST(testMacAddressComparisonWithDifferentValues);

    // === NULL ADDRESS TESTS ===
    DEFINE_TEST(testNullAddressIsAllZeros);
    DEFINE_TEST(testNullAddressComparisonWithZeroAddress);
    DEFINE_TEST(testNullAddressComparisonWithNonZeroAddress);

    // === MAC TO STRING CONVERSION TESTS ===
    DEFINE_TEST(testMacToStringConversionBasicFunctionality);
    DEFINE_TEST(testMacToStringConversionWithLowercaseHex);
    DEFINE_TEST(testMacToStringConversionWithUppercaseHex);
    DEFINE_TEST(testMacToStringConversionWithMixedValues);
    DEFINE_TEST(testMacToStringConversionWithAllZeros);
    DEFINE_TEST(testMacToStringConversionWithAllOnes);

    // === FRAME STRUCTURE TESTS ===
    DEFINE_TEST(testFrameStructureSize);
    DEFINE_TEST(testFrameHeaderSize);
    DEFINE_TEST(testFrameMtuValue);
    DEFINE_TEST(testFrameFieldAlignment);

    // === FRAME CREATION AND VALIDATION TESTS ===
    DEFINE_TEST(testFrameCreationWithValidParameters);
    DEFINE_TEST(testFrameDestinationAssignment);
    DEFINE_TEST(testFrameSourceAssignment);
    DEFINE_TEST(testFrameProtocolAssignment);
    DEFINE_TEST(testFramePayloadManipulation);
    DEFINE_TEST(testFramePayloadPatternValidation);

    // === EDGE CASES AND ROBUSTNESS TESTS ===
    DEFINE_TEST(testMacAddressWithEdgeValues);
    DEFINE_TEST(testFrameWithMaximumPayloadSize);
    DEFINE_TEST(testFrameWithDifferentProtocolValues);
}

/**
 * @brief Set up test environment before each test
 * 
 * Prepares the test environment with any necessary initialization.
 * Currently no specific setup is required for Ethernet tests.
 */
void EthernetTest::setUp() {
    // No specific setup needed for Ethernet tests
}

/**
 * @brief Clean up test environment after each test
 * 
 * Performs any necessary cleanup after test execution.
 * Currently no specific cleanup is required for Ethernet tests.
 */
void EthernetTest::tearDown() {
    // No specific cleanup needed for Ethernet tests
}

/**
 * @brief Helper method to create test MAC addresses
 * 
 * @param byte0 First byte of MAC address
 * @param byte1 Second byte of MAC address
 * @param byte2 Third byte of MAC address
 * @param byte3 Fourth byte of MAC address
 * @param byte4 Fifth byte of MAC address
 * @param byte5 Sixth byte of MAC address
 * @return Ethernet::Address with specified byte values
 * 
 * This utility method creates MAC addresses for testing purposes,
 * providing a consistent and readable way to generate test addresses.
 */
Ethernet::Address EthernetTest::createTestAddress(uint8_t byte0, uint8_t byte1, uint8_t byte2, 
                                                uint8_t byte3, uint8_t byte4, uint8_t byte5) {
    Ethernet::Address addr;
    addr.bytes[0] = byte0;
    addr.bytes[1] = byte1;
    addr.bytes[2] = byte2;
    addr.bytes[3] = byte3;
    addr.bytes[4] = byte4;
    addr.bytes[5] = byte5;
    return addr;
}

/**
 * @brief Helper method to create test Ethernet frames
 * 
 * @param dst Destination MAC address
 * @param src Source MAC address
 * @param protocol Protocol field value
 * @return Ethernet::Frame with specified header values
 * 
 * This utility method creates Ethernet frames for testing purposes,
 * initializing the header fields with the provided values.
 */
Ethernet::Frame EthernetTest::createTestFrame(const Ethernet::Address& dst, const Ethernet::Address& src, 
                                            uint16_t protocol) {
    Ethernet::Frame frame;
    frame.dst = dst;
    frame.src = src;
    frame.prot = protocol;
    return frame;
}

/**
 * @brief Helper method to fill frame payload with a test pattern
 * 
 * @param frame Reference to the frame to fill
 * @param pattern_start Starting value for the pattern (default: 0)
 * 
 * This utility method fills the frame payload with a sequential pattern
 * starting from the specified value, wrapping around at 256.
 */
void EthernetTest::fillFramePayload(Ethernet::Frame& frame, uint8_t pattern_start) {
    for (unsigned int i = 0; i < Ethernet::MTU; i++) {
        frame.payload[i] = static_cast<uint8_t>((pattern_start + i) % 256);
    }
}

/**
 * @brief Helper method to verify frame payload pattern
 * 
 * @param frame Reference to the frame to verify
 * @param expected_pattern_start Expected starting value for the pattern (default: 0)
 * @return true if payload matches expected pattern, false otherwise
 * 
 * This utility method verifies that the frame payload contains the
 * expected sequential pattern starting from the specified value.
 */
bool EthernetTest::verifyFramePayload(const Ethernet::Frame& frame, uint8_t expected_pattern_start) {
    for (unsigned int i = 0; i < Ethernet::MTU; i++) {
        uint8_t expected = static_cast<uint8_t>((expected_pattern_start + i) % 256);
        if (frame.payload[i] != expected) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Tests MAC address equality operator
 * 
 * Verifies that the equality operator (==) correctly identifies when two
 * MAC addresses contain identical byte values. This ensures that MAC
 * address comparisons work reliably for network operations.
 */
void EthernetTest::testMacAddressEquality() {
    auto addr1 = createTestAddress(0x00, 0x11, 0x22, 0x33, 0x44, 0x55);
    auto addr2 = createTestAddress(0x00, 0x11, 0x22, 0x33, 0x44, 0x55);
    
    assert_true(addr1 == addr2, "Identical MAC addresses should be equal");
}

/**
 * @brief Tests MAC address inequality operator
 * 
 * Verifies that the inequality operator (!=) correctly identifies when two
 * MAC addresses contain different byte values. This ensures that different
 * MAC addresses are properly distinguished in network operations.
 */
void EthernetTest::testMacAddressInequality() {
    auto addr1 = createTestAddress(0x00, 0x11, 0x22, 0x33, 0x44, 0x55);
    auto addr2 = createTestAddress(0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF);
    
    assert_true(addr1 != addr2, "Different MAC addresses should not be equal");
}

/**
 * @brief Tests MAC address comparison with same values
 * 
 * Verifies that multiple MAC addresses created with identical values
 * are correctly identified as equal, ensuring consistent behavior
 * across different instances.
 */
void EthernetTest::testMacAddressComparisonWithSameValues() {
    auto addr1 = createTestAddress(0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC);
    auto addr2 = createTestAddress(0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC);
    auto addr3 = createTestAddress(0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC);
    
    assert_true(addr1 == addr2, "Addresses with same values should be equal");
    assert_true(addr2 == addr3, "Addresses with same values should be equal");
    assert_true(addr1 == addr3, "Addresses with same values should be equal (transitivity)");
}

/**
 * @brief Tests MAC address comparison with different values
 * 
 * Verifies that MAC addresses with any differing bytes are correctly
 * identified as unequal, ensuring that even single-bit differences
 * are properly detected.
 */
void EthernetTest::testMacAddressComparisonWithDifferentValues() {
    auto addr1 = createTestAddress(0x00, 0x11, 0x22, 0x33, 0x44, 0x55);
    auto addr2 = createTestAddress(0x00, 0x11, 0x22, 0x33, 0x44, 0x56); // Last byte different
    auto addr3 = createTestAddress(0x01, 0x11, 0x22, 0x33, 0x44, 0x55); // First byte different
    
    assert_true(addr1 != addr2, "Addresses differing in last byte should not be equal");
    assert_true(addr1 != addr3, "Addresses differing in first byte should not be equal");
    assert_true(addr2 != addr3, "Different addresses should not be equal");
}

/**
 * @brief Tests that NULL_ADDRESS contains all zero bytes
 * 
 * Verifies that the predefined NULL_ADDRESS constant contains all zero
 * bytes as expected. This ensures that the null address represents an
 * invalid or uninitialized MAC address state correctly.
 */
void EthernetTest::testNullAddressIsAllZeros() {
    bool isAllZeros = true;
    for (unsigned int i = 0; i < Ethernet::MAC_SIZE; i++) {
        if (Ethernet::NULL_ADDRESS.bytes[i] != 0) {
            isAllZeros = false;
            break;
        }
    }
    assert_true(isAllZeros, "NULL_ADDRESS should have all bytes set to zero");
}

/**
 * @brief Tests NULL_ADDRESS comparison with manually created zero address
 * 
 * Verifies that the predefined NULL_ADDRESS is equal to a manually
 * created address with all zero bytes, ensuring consistency in
 * null address representation.
 */
void EthernetTest::testNullAddressComparisonWithZeroAddress() {
    auto zero_addr = createTestAddress(0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    
    assert_true(Ethernet::NULL_ADDRESS == zero_addr, "NULL_ADDRESS should equal manually created zero address");
}

/**
 * @brief Tests NULL_ADDRESS comparison with non-zero address
 * 
 * Verifies that the predefined NULL_ADDRESS is not equal to addresses
 * containing non-zero bytes, ensuring proper distinction between null
 * and valid addresses.
 */
void EthernetTest::testNullAddressComparisonWithNonZeroAddress() {
    auto non_zero_addr = createTestAddress(0x00, 0x00, 0x00, 0x00, 0x00, 0x01);
    
    assert_true(Ethernet::NULL_ADDRESS != non_zero_addr, "NULL_ADDRESS should not equal non-zero address");
}

/**
 * @brief Tests basic MAC to string conversion functionality
 * 
 * Verifies that the mac_to_string function correctly converts MAC addresses
 * to their standard string representation with colon separators and
 * uppercase hexadecimal digits.
 */
void EthernetTest::testMacToStringConversionBasicFunctionality() {
    auto addr = createTestAddress(0x00, 0x11, 0x22, 0x33, 0x44, 0x55);
    std::string mac_str = Ethernet::mac_to_string(addr);
    std::string expected = "00:11:22:33:44:55";
    
    assert_equal(expected, mac_str, "MAC address string conversion should work correctly");
}

/**
 * @brief Tests MAC to string conversion with lowercase hex values
 * 
 * Verifies that MAC addresses containing bytes that would typically
 * be represented with lowercase hex digits (a-f) are correctly
 * converted to uppercase in the string representation.
 */
void EthernetTest::testMacToStringConversionWithLowercaseHex() {
    auto addr = createTestAddress(0xab, 0xcd, 0xef, 0x01, 0x23, 0x45);
    std::string mac_str = Ethernet::mac_to_string(addr);
    std::string expected = "AB:CD:EF:01:23:45";
    
    assert_equal(expected, mac_str, "MAC string conversion should handle lowercase hex correctly");
}

/**
 * @brief Tests MAC to string conversion with uppercase hex values
 * 
 * Verifies that MAC addresses containing bytes that are naturally
 * represented with uppercase hex digits are correctly converted
 * to the standard string format.
 */
void EthernetTest::testMacToStringConversionWithUppercaseHex() {
    auto addr = createTestAddress(0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF);
    std::string mac_str = Ethernet::mac_to_string(addr);
    std::string expected = "AA:BB:CC:DD:EE:FF";
    
    assert_equal(expected, mac_str, "MAC string conversion should handle uppercase hex correctly");
}

/**
 * @brief Tests MAC to string conversion with mixed hex values
 * 
 * Verifies that MAC addresses containing a mix of numeric and
 * alphabetic hex digits are correctly converted to the standard
 * string representation.
 */
void EthernetTest::testMacToStringConversionWithMixedValues() {
    auto addr = createTestAddress(0x1A, 0x2B, 0x3C, 0x4D, 0x5E, 0x6F);
    std::string mac_str = Ethernet::mac_to_string(addr);
    std::string expected = "1A:2B:3C:4D:5E:6F";
    
    assert_equal(expected, mac_str, "MAC string conversion should handle mixed hex values correctly");
}

/**
 * @brief Tests MAC to string conversion with all zero bytes
 * 
 * Verifies that the NULL_ADDRESS (all zeros) is correctly converted
 * to its string representation, ensuring proper handling of the
 * null address case.
 */
void EthernetTest::testMacToStringConversionWithAllZeros() {
    std::string mac_str = Ethernet::mac_to_string(Ethernet::NULL_ADDRESS);
    std::string expected = "00:00:00:00:00:00";
    
    assert_equal(expected, mac_str, "NULL_ADDRESS string conversion should work correctly");
}

/**
 * @brief Tests MAC to string conversion with all one bytes
 * 
 * Verifies that MAC addresses with all bytes set to 0xFF (broadcast
 * address) are correctly converted to their string representation.
 */
void EthernetTest::testMacToStringConversionWithAllOnes() {
    auto addr = createTestAddress(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
    std::string mac_str = Ethernet::mac_to_string(addr);
    std::string expected = "FF:FF:FF:FF:FF:FF";
    
    assert_equal(expected, mac_str, "Broadcast address string conversion should work correctly");
}

/**
 * @brief Tests Ethernet frame structure size
 * 
 * Verifies that the Ethernet::Frame structure has the correct total size,
 * which should be the sum of the header size and the maximum transmission
 * unit (MTU). This ensures proper memory layout for network operations.
 */
void EthernetTest::testFrameStructureSize() {
    size_t expected_size = Ethernet::HEADER_SIZE + Ethernet::MTU;
    size_t actual_size = sizeof(Ethernet::Frame);
    
    assert_equal(expected_size, actual_size, "Ethernet frame size should be header size + MTU");
}

/**
 * @brief Tests Ethernet header size constant
 * 
 * Verifies that the HEADER_SIZE constant correctly represents the size
 * of the Ethernet header (destination, source, and protocol fields).
 */
void EthernetTest::testFrameHeaderSize() {
    size_t expected_header_size = sizeof(Ethernet::Address) + sizeof(Ethernet::Address) + sizeof(uint16_t);
    
    assert_equal(expected_header_size, Ethernet::HEADER_SIZE, "HEADER_SIZE should match actual header fields size");
}

/**
 * @brief Tests Ethernet MTU value
 * 
 * Verifies that the MTU constant has a reasonable value that is greater
 * than zero and within expected bounds for Ethernet frames.
 */
void EthernetTest::testFrameMtuValue() {
    assert_true(Ethernet::MTU > 0, "MTU should be greater than zero");
    assert_true(Ethernet::MTU <= 1500, "MTU should not exceed standard Ethernet MTU");
}

/**
 * @brief Tests Ethernet frame field alignment
 * 
 * Verifies that the frame structure fields are properly aligned and
 * accessible without padding issues that could affect network operations.
 */
void EthernetTest::testFrameFieldAlignment() {
    Ethernet::Frame frame;
    
    // Test that all fields are accessible
    frame.dst = createTestAddress(0x01, 0x02, 0x03, 0x04, 0x05, 0x06);
    frame.src = createTestAddress(0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C);
    frame.prot = 0x0800;
    frame.payload[0] = 0xAA;
    frame.payload[Ethernet::MTU - 1] = 0xBB;
    
    // Verify assignments worked correctly
    assert_true(frame.dst.bytes[0] == 0x01 && frame.dst.bytes[5] == 0x06, "Destination field should be accessible");
    assert_true(frame.src.bytes[0] == 0x07 && frame.src.bytes[5] == 0x0C, "Source field should be accessible");
    assert_equal(static_cast<uint16_t>(0x0800), frame.prot, "Protocol field should be accessible");
    assert_equal(static_cast<uint8_t>(0xAA), frame.payload[0], "Payload start should be accessible");
    assert_equal(static_cast<uint8_t>(0xBB), frame.payload[Ethernet::MTU - 1], "Payload end should be accessible");
}

/**
 * @brief Tests frame creation with valid parameters
 * 
 * Verifies that Ethernet frames can be created and initialized with
 * valid destination, source, and protocol values, ensuring basic
 * frame construction functionality works correctly.
 */
void EthernetTest::testFrameCreationWithValidParameters() {
    auto dst_addr = createTestAddress(0x00, 0x11, 0x22, 0x33, 0x44, 0x55);
    auto src_addr = createTestAddress(0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF);
    uint16_t protocol = 0x0800; // IPv4
    
    auto frame = createTestFrame(dst_addr, src_addr, protocol);
    
    assert_true(frame.dst == dst_addr, "Frame destination should match");
    assert_true(frame.src == src_addr, "Frame source should match");
    assert_equal(protocol, frame.prot, "Frame protocol should match");
}

/**
 * @brief Tests frame destination address assignment
 * 
 * Verifies that the destination field of Ethernet frames can be
 * properly assigned and retrieved, ensuring correct handling of
 * destination addressing.
 */
void EthernetTest::testFrameDestinationAssignment() {
    Ethernet::Frame frame;
    auto dst_addr = createTestAddress(0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC);
    
    frame.dst = dst_addr;
    
    assert_true(frame.dst == dst_addr, "Frame destination assignment should work correctly");
    
    // Verify individual bytes
    for (unsigned int i = 0; i < Ethernet::MAC_SIZE; i++) {
        assert_equal(dst_addr.bytes[i], frame.dst.bytes[i], "Destination bytes should match");
    }
}

/**
 * @brief Tests frame source address assignment
 * 
 * Verifies that the source field of Ethernet frames can be properly
 * assigned and retrieved, ensuring correct handling of source addressing.
 */
void EthernetTest::testFrameSourceAssignment() {
    Ethernet::Frame frame;
    auto src_addr = createTestAddress(0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54);
    
    frame.src = src_addr;
    
    assert_true(frame.src == src_addr, "Frame source assignment should work correctly");
    
    // Verify individual bytes
    for (unsigned int i = 0; i < Ethernet::MAC_SIZE; i++) {
        assert_equal(src_addr.bytes[i], frame.src.bytes[i], "Source bytes should match");
    }
}

/**
 * @brief Tests frame protocol field assignment
 * 
 * Verifies that the protocol field of Ethernet frames can be properly
 * assigned with different protocol values, ensuring correct protocol
 * identification for upper layer processing.
 */
void EthernetTest::testFrameProtocolAssignment() {
    Ethernet::Frame frame;
    
    // Test IPv4 protocol
    frame.prot = 0x0800;
    assert_equal(static_cast<uint16_t>(0x0800), frame.prot, "IPv4 protocol assignment should work");
    
    // Test IPv6 protocol
    frame.prot = 0x86DD;
    assert_equal(static_cast<uint16_t>(0x86DD), frame.prot, "IPv6 protocol assignment should work");
    
    // Test ARP protocol
    frame.prot = 0x0806;
    assert_equal(static_cast<uint16_t>(0x0806), frame.prot, "ARP protocol assignment should work");
}

/**
 * @brief Tests frame payload manipulation
 * 
 * Verifies that the payload field of Ethernet frames can be written to
 * and read from correctly, ensuring proper data handling for network
 * packet processing.
 */
void EthernetTest::testFramePayloadManipulation() {
    Ethernet::Frame frame;
    
    // Fill payload with test pattern
    fillFramePayload(frame, 0);
    
    // Verify the pattern was written correctly
    assert_true(verifyFramePayload(frame, 0), "Frame payload should match test pattern");
    
    // Test individual byte access
    frame.payload[0] = 0xAA;
    frame.payload[Ethernet::MTU - 1] = 0xBB;
    
    assert_equal(static_cast<uint8_t>(0xAA), frame.payload[0], "First payload byte should be accessible");
    assert_equal(static_cast<uint8_t>(0xBB), frame.payload[Ethernet::MTU - 1], "Last payload byte should be accessible");
}

/**
 * @brief Tests frame payload pattern validation
 * 
 * Verifies that complex payload patterns can be written to and verified
 * correctly, ensuring reliable data integrity for network operations.
 */
void EthernetTest::testFramePayloadPatternValidation() {
    Ethernet::Frame frame;
    
    // Test with different starting patterns
    fillFramePayload(frame, 100);
    assert_true(verifyFramePayload(frame, 100), "Frame payload should match pattern starting at 100");
    
    fillFramePayload(frame, 200);
    assert_true(verifyFramePayload(frame, 200), "Frame payload should match pattern starting at 200");
    
    // Test that wrong pattern is detected
    fillFramePayload(frame, 50);
    assert_false(verifyFramePayload(frame, 51), "Wrong pattern should be detected");
}

/**
 * @brief Tests MAC addresses with edge case values
 * 
 * Verifies that MAC addresses work correctly with boundary values
 * such as all zeros, all ones, and other edge cases that might
 * occur in real network scenarios.
 */
void EthernetTest::testMacAddressWithEdgeValues() {
    // Test with minimum values
    auto min_addr = createTestAddress(0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    assert_true(min_addr == Ethernet::NULL_ADDRESS, "Minimum address should equal NULL_ADDRESS");
    
    // Test with maximum values
    auto max_addr = createTestAddress(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
    std::string max_str = Ethernet::mac_to_string(max_addr);
    assert_equal(std::string("FF:FF:FF:FF:FF:FF"), max_str, "Maximum address string should be correct");
    
    // Test with mixed edge values
    auto mixed_addr = createTestAddress(0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF);
    std::string mixed_str = Ethernet::mac_to_string(mixed_addr);
    assert_equal(std::string("00:FF:00:FF:00:FF"), mixed_str, "Mixed edge address string should be correct");
}

/**
 * @brief Tests frame with maximum payload size
 * 
 * Verifies that Ethernet frames can handle the maximum payload size
 * correctly, ensuring that all payload bytes are accessible and
 * can be used for data transmission.
 */
void EthernetTest::testFrameWithMaximumPayloadSize() {
    Ethernet::Frame frame;
    
    // Fill entire payload
    for (unsigned int i = 0; i < Ethernet::MTU; i++) {
        frame.payload[i] = static_cast<uint8_t>(i & 0xFF);
    }
    
    // Verify all bytes are accessible
    bool all_correct = true;
    for (unsigned int i = 0; i < Ethernet::MTU; i++) {
        if (frame.payload[i] != static_cast<uint8_t>(i & 0xFF)) {
            all_correct = false;
            break;
        }
    }
    
    assert_true(all_correct, "All payload bytes should be accessible and correct");
}

/**
 * @brief Tests frame with different protocol values
 * 
 * Verifies that Ethernet frames can handle various protocol field
 * values correctly, ensuring proper support for different upper
 * layer protocols.
 */
void EthernetTest::testFrameWithDifferentProtocolValues() {
    Ethernet::Frame frame;
    
    // Test common protocol values
    std::vector<uint16_t> protocols = {
        0x0800, // IPv4
        0x86DD, // IPv6
        0x0806, // ARP
        0x8100, // VLAN
        0x88CC, // LLDP
        0x0000, // Minimum value
        0xFFFF  // Maximum value
    };
    
    for (uint16_t protocol : protocols) {
        frame.prot = protocol;
        assert_equal(protocol, frame.prot, "Protocol field should handle various values correctly");
    }
}

// Main function
int main() {
    TEST_INIT("EthernetTest");
    EthernetTest test;
    test.run();
    return 0;
} 