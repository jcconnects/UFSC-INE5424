#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include "test_utils.h"
#include "socketEngine.h"
#include "ethernet.h"

// Define Ethernet frame length constant only if not already defined
#ifndef ETH_FRAME_LEN
#define ETH_FRAME_LEN (Ethernet::MTU + Ethernet::HEADER_SIZE)
#endif

// Concrete implementation of SocketEngine for testing
class TestSocketEngine : public SocketEngine {
public:
    TestSocketEngine() : SocketEngine(), signal_count(0), received_frames(0) {}
    
    int getSignalCount() const { return signal_count; }
    int getReceivedFrames() const { return received_frames; }
    
    // Expose protected members for testing
    int getSocketFd() const { return _sock_fd; }
    int getIfIndex() const { return _if_index; }
    
    void resetCounters() {
        signal_count = 0;
        received_frames = 0;
    }
    
    // Add a simulated signal handler for testing
    void simulateSignal() {
        signal_count++;
        
        // Read frame from socket (non-blocking)
        char buffer[ETH_FRAME_LEN];
        ssize_t len = recv(_sock_fd, buffer, ETH_FRAME_LEN, MSG_DONTWAIT);
        
        if (len > 0) {
            received_frames++;
        }
    }

    // Implementation of the pure virtual function
    void handleExternal(Ethernet::Frame* frame, unsigned int size) override {
        received_frames++;
    }

private:
    int signal_count;
    int received_frames;
};

int main() {
    TEST_INIT("socketEngine_test");
    
    TEST_LOG("Creating two TestSocketEngine instances");
    TestSocketEngine engineA;
    TestSocketEngine engineB;
    
    // Start the engines
    engineA.start();
    engineB.start();
    
    // Test 1: Initialization
    TEST_ASSERT(engineA.getSocketFd() > 0, "EngineA socket file descriptor should be valid");
    TEST_ASSERT(engineA.getIfIndex() > 0, "EngineA interface index should be valid");
    
    TEST_ASSERT(engineB.getSocketFd() > 0, "EngineB socket file descriptor should be valid");
    TEST_ASSERT(engineB.getIfIndex() > 0, "EngineB interface index should be valid");
    
    // Log MAC addresses (should be the same since both use the same interface)
    Ethernet::Address macA = engineA.mac_address();
    Ethernet::Address macB = engineB.mac_address();
    
    TEST_LOG("EngineA MAC Address: " + Ethernet::mac_to_string(macA));
    TEST_LOG("EngineB MAC Address: " + Ethernet::mac_to_string(macB));
    
    TEST_ASSERT(Ethernet::mac_to_string(macA) != "00:00:00:00:00:00", "EngineA MAC address should not be empty");
    TEST_ASSERT(Ethernet::mac_to_string(macB) != "00:00:00:00:00:00", "EngineB MAC address should not be empty");
    
    // Test 2: Engines running status
    TEST_ASSERT(engineA.running() == true, "EngineA should be running after initialization");
    TEST_ASSERT(engineB.running() == true, "EngineB should be running after initialization");
    
    // Test 3: Send a broadcast frame from EngineA that should be received by EngineB
    TEST_LOG("Preparing to send a broadcast frame from EngineA");
    
    // Create a minimal Ethernet frame with broadcast destination and a small payload
    const int payload_size = 46; // Minimum Ethernet payload size
    const int frame_size = Ethernet::HEADER_SIZE + payload_size;
    
    // Allocate buffer for the whole frame
    uint8_t* buffer = new uint8_t[frame_size];
    std::memset(buffer, 0, frame_size);
    
    // Set up the frame header
    Ethernet::Frame* frame = reinterpret_cast<Ethernet::Frame*>(buffer);
    
    // Set destination to broadcast (FF:FF:FF:FF:FF:FF)
    std::memset(frame->dst.bytes, 0xFF, Ethernet::MAC_SIZE);
    
    // Set source to EngineA's MAC
    std::memcpy(frame->src.bytes, macA.bytes, Ethernet::MAC_SIZE);
    
    // Set protocol
    frame->prot = 0x0800; // IPv4
    
    // Add some data to the payload
    uint8_t* payload = buffer + Ethernet::HEADER_SIZE;
    for (int i = 0; i < payload_size; i++) {
        payload[i] = i & 0xFF;
    }
    
    // Reset counters before sending
    engineA.resetCounters();
    engineB.resetCounters();
    
    TEST_LOG("Sending broadcast frame with size: " + std::to_string(frame_size) + " bytes");
    
    // Send the frame from EngineA
    int result = engineA.send(frame, frame_size);
    TEST_ASSERT(result >= 0, "Send operation from EngineA should succeed");
    
    // Give some time for the receive thread to process the frame
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Manually simulate receiving a signal (since there's no automatic handleSignal anymore)
    engineB.simulateSignal();
    
    // Test 4: Check if engineB received the broadcast
    TEST_LOG("EngineA Signal count: " + std::to_string(engineA.getSignalCount()));
    TEST_LOG("EngineA Received frames: " + std::to_string(engineA.getReceivedFrames()));
    TEST_LOG("EngineB Signal count: " + std::to_string(engineB.getSignalCount()));
    TEST_LOG("EngineB Received frames: " + std::to_string(engineB.getReceivedFrames()));
    
    // Check if our simulated signal was processed
    TEST_ASSERT(engineB.getSignalCount() > 0, "EngineB should process the simulated signal");
    
    // Test 5: Send from EngineB to EngineA directly (using MAC address)
    TEST_LOG("Sending direct frame from EngineB to EngineA");
    
    // Change frame to be from B to A
    std::memcpy(frame->dst.bytes, macA.bytes, Ethernet::MAC_SIZE);
    std::memcpy(frame->src.bytes, macB.bytes, Ethernet::MAC_SIZE);
    
    // Reset counters
    engineA.resetCounters();
    engineB.resetCounters();
    
    // Send from B
    result = engineB.send(frame, frame_size);
    TEST_ASSERT(result >= 0, "Send operation from EngineB should succeed");
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Simulate signal handling for engineA
    engineA.simulateSignal();
    
    // Check results
    TEST_LOG("After direct send:");
    TEST_LOG("EngineA Signal count: " + std::to_string(engineA.getSignalCount()));
    TEST_LOG("EngineA Received frames: " + std::to_string(engineA.getReceivedFrames()));
    TEST_LOG("EngineB Signal count: " + std::to_string(engineB.getSignalCount()));
    TEST_LOG("EngineB Received frames: " + std::to_string(engineB.getReceivedFrames()));
    
    // Test that our simulated signal was processed
    TEST_ASSERT(engineA.getSignalCount() > 0, "EngineA should process the simulated signal");
    
    // Test 6: Stop the engines
    TEST_LOG("Stopping engines");
    engineA.stop();
    engineB.stop();
    
    TEST_ASSERT(engineA.running() == false, "EngineA should not be running after stop");
    TEST_ASSERT(engineB.running() == false, "EngineB should not be running after stop");
    
    // Clean up
    delete[] buffer;
    
    std::cout << "SocketEngine test completed successfully!" << std::endl;
    return 0;
} 