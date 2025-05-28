#include <iostream>
#include <cstring>
#include "test_utils.h"
#include "network/ethernet.h"

int main() {
    TEST_INIT("ethernet_test");
    
    // Test 1: MAC address comparisons
    Ethernet::Address addr1 = {{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    Ethernet::Address addr2 = {{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    Ethernet::Address addr3 = {{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};
    
    TEST_ASSERT(addr1 == addr2, "Identical MAC addresses should be equal");
    TEST_ASSERT(addr1 != addr3, "Different MAC addresses should not be equal");
    TEST_ASSERT(addr2 != addr3, "Different MAC addresses should not be equal");
    
    // Test 2: NULL_ADDRESS should be all zeros
    bool isAllZeros = true;
    for (unsigned int i = 0; i < Ethernet::MAC_SIZE; i++) {
        if (Ethernet::NULL_ADDRESS.bytes[i] != 0) {
            isAllZeros = false;
            break;
        }
    }
    TEST_ASSERT(isAllZeros, "NULL_ADDRESS should have all bytes set to zero");
    
    // Test 3: MAC to string conversion
    std::string mac1 = Ethernet::mac_to_string(addr1);
    std::string expectedMac1 = "00:11:22:33:44:55";
    TEST_ASSERT(mac1 == expectedMac1, "MAC address string conversion should work correctly");
    
    std::string mac3 = Ethernet::mac_to_string(addr3);
    std::string expectedMac3 = "AA:BB:CC:DD:EE:FF";
    TEST_ASSERT(mac3 == expectedMac3, "MAC address string conversion should work correctly for different address");
    
    // Test 4: Frame structure size
    TEST_ASSERT(sizeof(Ethernet::Frame) == Ethernet::HEADER_SIZE + Ethernet::MTU, 
                "Ethernet frame size should be header size + MTU");
    
    // Test 5: Create and validate a frame
    Ethernet::Frame frame;
    frame.dst = addr1;
    frame.src = addr3;
    frame.prot = 0x0800; // IPv4
    
    // Fill the payload with a pattern
    for (unsigned int i = 0; i < Ethernet::MTU; i++) {
        frame.payload[i] = i % 256;
    }
    
    // Verify frame structure
    TEST_ASSERT(frame.dst == addr1, "Frame destination should match");
    TEST_ASSERT(frame.src == addr3, "Frame source should match");
    TEST_ASSERT(frame.prot == 0x0800, "Frame protocol should match");
    
    // Verify frame payload
    bool payloadCorrect = true;
    for (unsigned int i = 0; i < Ethernet::MTU; i++) {
        if (frame.payload[i] != (i % 256)) {
            payloadCorrect = false;
            break;
        }
    }
    TEST_ASSERT(payloadCorrect, "Frame payload should match the pattern");
    
    std::cout << "Ethernet test passed successfully!" << std::endl;
    return 0;
} 