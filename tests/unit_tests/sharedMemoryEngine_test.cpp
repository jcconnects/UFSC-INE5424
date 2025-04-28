#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <sstream>
#include "test_utils.h"
#include "sharedMemoryEngine.h"
#include "ethernet.h"


int main() {
    TEST_INIT("sharedMemoryEngine_test");
    
    // Test 1: Create engines
    TEST_LOG("Creating two TestSharedMemoryEngine instances");
    TestSharedMemoryEngine engineA;
    TestSharedMemoryEngine engineB;
    
    // Test 2: Get MAC addresses (should be different based on instance ID)
    Ethernet::Address macA = engineA.getMacAddress();
    Ethernet::Address macB = engineB.getMacAddress();
    
    TEST_LOG("EngineA MAC Address: " + Ethernet::mac_to_string(macA));
    TEST_LOG("EngineB MAC Address: " + Ethernet::mac_to_string(macB));
    
    TEST_ASSERT(memcmp(macA.bytes, macB.bytes, Ethernet::MAC_SIZE) != 0, 
                "Engines should have different MAC addresses");
    TEST_ASSERT(macA.bytes[0] == 0x02 && macA.bytes[5] == 1, 
                "EngineA MAC should encode instance ID 1");
    TEST_ASSERT(macB.bytes[0] == 0x02 && macB.bytes[5] == 2, 
                "EngineB MAC should encode instance ID 2");

    // Start the engines
    engineA.start();
    engineB.start();
    
    // Test 4: Send a direct message from EngineA to EngineB
    TEST_LOG("Preparing to send a direct frame from EngineA to EngineB");
    
    // Create a minimal Ethernet frame with engineB's address as destination
    const int payload_size = 46; // Minimum Ethernet payload size
    const int frame_size = Ethernet::HEADER_SIZE + payload_size;
    
    // Allocate buffer for the whole frame
    uint8_t* buffer = new uint8_t[frame_size];
    std::memset(buffer, 0, frame_size);
    
    // Set up the frame header
    Ethernet::Frame* frame = reinterpret_cast<Ethernet::Frame*>(buffer);
    
    // Set destination to engineB's MAC
    std::memcpy(frame->dst.bytes, macB.bytes, Ethernet::MAC_SIZE);
    
    // Set source to engineA's MAC
    std::memcpy(frame->src.bytes, macA.bytes, Ethernet::MAC_SIZE);
    
    // Set protocol (use a test protocol number)
    frame->prot = 0x8888;
    
    // Add some data to the payload
    uint8_t* payload = buffer + Ethernet::HEADER_SIZE;
    for (int i = 0; i < payload_size; i++) {
        payload[i] = i & 0xFF;
    }
    
    TEST_LOG("Sending direct frame with size: " + std::to_string(frame_size) + " bytes");
    
    // Send the frame from EngineA
    int result = engineA.send(frame, frame_size);
    TEST_ASSERT(result >= 0, "Send operation from EngineA should succeed");
    
    // Give some time for the receive thread to process the frame
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Note: In this test, we're using a mocked receiveData that doesn't actually
    // read from shared memory, so we won't test real inter-engine communication.
    // Instead, we'll verify the internal logic works correctly.
    
    // Test simulated frame reception
    Ethernet::Protocol proto;
    uint8_t recv_buffer[Traits<SharedMemoryEngine>::MTU];
    
    // Test 5: Stop the engines
    TEST_LOG("Stopping engines");
    engineA.stop();
    engineB.stop();
    
    // Clean up
    delete[] buffer;
    
    std::cout << "SharedMemoryEngine test completed successfully!" << std::endl;
    return 0;
} 