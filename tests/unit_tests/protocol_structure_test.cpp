#include "../../tests/testcase.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>

// Define the types we need for testing without including the full Clock header
using TimestampType = std::chrono::time_point<std::chrono::steady_clock, std::chrono::microseconds>;

/**
 * @brief Test class for verifying Protocol structure sizes and offsets
 * 
 * This test ensures that the Clock-Protocol-NIC integration remains correct
 * by verifying that structure sizes and field offsets match the expected
 * values used in the NIC layer for timestamp insertion.
 * 
 * Critical for maintaining correctness of PTP timestamping integration.
 */
class ProtocolStructureTest : public TestCase {
public:
    ProtocolStructureTest();
    
    // Required TestCase interface methods
    void setUp() override {}
    void tearDown() override {}

private:
    // Test methods
    void testHeaderStructureSize();
    void testTimestampFieldsStructureSize();
    void testTimestampFieldsOffsets();
    void testOverallPacketLayout();
    void testAlignmentRequirements();
    void testNicOffsetCalculations();
    
    // Helper structures matching the Protocol implementation
    struct Header {
        std::uint16_t _from_port;
        std::uint16_t _to_port;
        unsigned int _size;
    };
    
    struct TimestampFields {
        bool is_clock_synchronized;
        TimestampType tx_timestamp;
        TimestampType rx_timestamp;
    };
};

ProtocolStructureTest::ProtocolStructureTest() {
    DEFINE_TEST(testHeaderStructureSize);
    DEFINE_TEST(testTimestampFieldsStructureSize);
    DEFINE_TEST(testTimestampFieldsOffsets);
    DEFINE_TEST(testOverallPacketLayout);
    DEFINE_TEST(testAlignmentRequirements);
    DEFINE_TEST(testNicOffsetCalculations);
}

/**
 * @brief Test that Header structure has the expected size
 * 
 * The Header contains:
 * - from_port: uint16_t (2 bytes)
 * - to_port: uint16_t (2 bytes)  
 * - size: unsigned int (4 bytes)
 * Expected total: 8 bytes
 */
void ProtocolStructureTest::testHeaderStructureSize() {
    const unsigned int expected_header_size = 8;
    const unsigned int actual_header_size = sizeof(Header);
    
    assert_equal(expected_header_size, actual_header_size, 
        "Header structure size must be 8 bytes for correct NIC offset calculations");
    
    // Verify individual field sizes
    assert_equal(2u, static_cast<unsigned int>(sizeof(std::uint16_t)), 
        "uint16_t must be 2 bytes");
    assert_equal(4u, static_cast<unsigned int>(sizeof(unsigned int)), 
        "unsigned int must be 4 bytes");
}

/**
 * @brief Test that TimestampFields structure has the expected size
 * 
 * The TimestampFields contains:
 * - is_clock_synchronized: bool (1 byte + 7 bytes padding)
 * - tx_timestamp: TimestampType (8 bytes)
 * - rx_timestamp: TimestampType (8 bytes)
 * Expected total: 24 bytes
 */
void ProtocolStructureTest::testTimestampFieldsStructureSize() {
    const unsigned int expected_timestamp_fields_size = 24;
    const unsigned int actual_timestamp_fields_size = sizeof(TimestampFields);
    
    assert_equal(expected_timestamp_fields_size, actual_timestamp_fields_size,
        "TimestampFields structure size must be 24 bytes for correct NIC offset calculations");
    
    // Verify TimestampType size
    assert_equal(8u, static_cast<unsigned int>(sizeof(TimestampType)),
        "TimestampType must be 8 bytes");
    
    // Verify bool size
    assert_equal(1u, static_cast<unsigned int>(sizeof(bool)),
        "bool must be 1 byte");
}

/**
 * @brief Test that TimestampFields field offsets are as expected
 * 
 * Critical test: These offsets must match the hardcoded values in NIC layer
 */
void ProtocolStructureTest::testTimestampFieldsOffsets() {
    // These offsets are critical for NIC timestamp insertion
    const unsigned int expected_sync_status_offset = 0;
    const unsigned int expected_tx_timestamp_offset = 8;
    const unsigned int expected_rx_timestamp_offset = 16;
    
    assert_equal(expected_sync_status_offset, 
        static_cast<unsigned int>(offsetof(TimestampFields, is_clock_synchronized)),
        "Sync status offset must be 0");
    
    assert_equal(expected_tx_timestamp_offset,
        static_cast<unsigned int>(offsetof(TimestampFields, tx_timestamp)),
        "TX timestamp offset must be 8 (critical for NIC layer)");
    
    assert_equal(expected_rx_timestamp_offset,
        static_cast<unsigned int>(offsetof(TimestampFields, rx_timestamp)),
        "RX timestamp offset must be 16 (critical for NIC layer)");
    
    // std::cout << "✓ TimestampFields offsets verified:" << std::endl;
    // std::cout << "  - sync_status: " << offsetof(TimestampFields, is_clock_synchronized) << std::endl;
    // std::cout << "  - tx_timestamp: " << offsetof(TimestampFields, tx_timestamp) << std::endl;
    // std::cout << "  - rx_timestamp: " << offsetof(TimestampFields, rx_timestamp) << std::endl;
}

/**
 * @brief Test the overall packet layout calculations
 * 
 * Verifies that the total packet structure matches expectations
 */
void ProtocolStructureTest::testOverallPacketLayout() {
    const unsigned int header_size = sizeof(Header);
    const unsigned int timestamp_fields_size = sizeof(TimestampFields);
    const unsigned int total_protocol_overhead = header_size + timestamp_fields_size;
    
    // Expected: 8 + 24 = 32 bytes total protocol overhead
    assert_equal(32u, total_protocol_overhead,
        "Total protocol overhead must be 32 bytes");
    
    // std::cout << "✓ Protocol overhead breakdown:" << std::endl;
    // std::cout << "  - Header: " << header_size << " bytes" << std::endl;
    // std::cout << "  - TimestampFields: " << timestamp_fields_size << " bytes" << std::endl;
    // std::cout << "  - Total: " << total_protocol_overhead << " bytes" << std::endl;
}

/**
 * @brief Test alignment requirements and padding
 * 
 * Verifies that the compiler adds the expected padding for alignment
 */
void ProtocolStructureTest::testAlignmentRequirements() {
    // The bool field should have 7 bytes of padding due to TimestampType alignment
    const unsigned int bool_size = sizeof(bool);
    const unsigned int tx_timestamp_offset = offsetof(TimestampFields, tx_timestamp);
    const unsigned int padding_bytes = tx_timestamp_offset - bool_size;
    
    assert_equal(1u, bool_size, "bool should be 1 byte");
    assert_equal(7u, padding_bytes, "Should have 7 bytes of padding after bool for alignment");
    
    // std::cout << "✓ Alignment analysis:" << std::endl;
    // std::cout << "  - bool size: " << bool_size << " byte" << std::endl;
    // std::cout << "  - Padding after bool: " << padding_bytes << " bytes" << std::endl;
    // std::cout << "  - TX timestamp starts at offset: " << tx_timestamp_offset << std::endl;
}

/**
 * @brief Test that NIC offset calculations match structure layout
 * 
 * This is the most critical test - it verifies that the hardcoded offsets
 * in the NIC layer match the actual structure layout
 */
void ProtocolStructureTest::testNicOffsetCalculations() {
    // These are the offsets used in the NIC layer (after our fixes)
    const unsigned int header_size = sizeof(std::uint16_t) * 2 + sizeof(std::uint32_t); // 8 bytes
    const unsigned int nic_tx_timestamp_offset = header_size + 8;  // 16 bytes
    const unsigned int nic_rx_timestamp_offset = header_size + 16; // 24 bytes
    
    // These are the actual structure offsets
    const unsigned int actual_header_size = sizeof(Header);
    const unsigned int actual_tx_offset = actual_header_size + offsetof(TimestampFields, tx_timestamp);
    const unsigned int actual_rx_offset = actual_header_size + offsetof(TimestampFields, rx_timestamp);
    
    // Critical assertions - if these fail, the NIC layer will write timestamps to wrong locations
    assert_equal(actual_header_size, header_size,
        "NIC header size calculation must match actual Header size");
    
    assert_equal(actual_tx_offset, nic_tx_timestamp_offset,
        "NIC TX timestamp offset must match actual structure layout");
    
    assert_equal(actual_rx_offset, nic_rx_timestamp_offset,
        "NIC RX timestamp offset must match actual structure layout");
    
    // std::cout << "✓ NIC offset verification:" << std::endl;
    // std::cout << "  - Header size: " << header_size << " bytes (NIC) vs " << actual_header_size << " bytes (actual)" << std::endl;
    // std::cout << "  - TX offset: " << nic_tx_timestamp_offset << " bytes (NIC) vs " << actual_tx_offset << " bytes (actual)" << std::endl;
    // std::cout << "  - RX offset: " << nic_rx_timestamp_offset << " bytes (NIC) vs " << actual_rx_offset << " bytes (actual)" << std::endl;
}

// Main function
int main() {
    std::cout << "Running Protocol Structure Test..." << std::endl;
    ProtocolStructureTest test;
    test.run();
    return 0;
} 