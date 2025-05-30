#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <sstream>
#include "../test_utils.h"
#include "sharedMemoryEngine.h"
#include "ethernet.h"


int main() {
    TEST_INIT("sharedMemoryEngine_test");
    
    // Test 1: Create engines
    TEST_LOG("Creating two TestSharedMemoryEngine instances");
    SharedMemoryEngine engineA;
    SharedMemoryEngine engineB;

    // Start the engines
    engineA.start();
    engineB.start();
    
    // Test 2: Get MAC addresses (should be different based on instance ID)
    Ethernet::Address macA = engineA.getMacAddress();
    Ethernet::Address macB = engineB.getMacAddress();
    
    TEST_LOG("EngineA MAC Address: " + Ethernet::mac_to_string(macA));
    TEST_LOG("EngineB MAC Address: " + Ethernet::mac_to_string(macB));

    Ethernet::Frame* false_frame = nullptr;
    // Send the frame from EngineA
    int result = engineA.send(false_frame, 1);
    TEST_ASSERT(result >= 0, "Send operation from EngineA should succeed");
    
    // Give some time for the receive thread to process the frame
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Test 5: Stop the engines
    TEST_LOG("Stopping engines");
    engineA.stop();
    engineB.stop();

    
    TEST_LOG("SharedMemoryEngine test completed successfully!");
    return 0;
} 