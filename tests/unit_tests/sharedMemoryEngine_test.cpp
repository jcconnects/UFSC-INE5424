#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <sstream>
#include "test_utils.h"
#include "sharedMemoryEngine.h"
#include "ethernet.h"

// Concrete implementation of SharedMemoryEngine for testing
class TestSharedMemoryEngine : public SharedMemoryEngine {
public:
    TestSharedMemoryEngine(SharedMemoryEngine::ComponentId id) 
        : SharedMemoryEngine(id), signal_count(0), received_frames(0) {}
    
    int getSignalCount() const { return signal_count; }
    int getReceivedFrames() const { return received_frames; }
    
    SharedFrame getLastReceivedFrame() const { 
        return last_received_frame; 
    }
    
    void resetCounters() {
        signal_count = 0;
        received_frames = 0;
        log_messages.clear();
    }
    
    // Get log messages from the handleSignal method
    std::vector<std::string> getLogMessages() const {
        return log_messages;
    }

protected:
    // Implementation of pure virtual method
    void handleSignal() override {
        signal_count++;
        
        SharedFrame frame;
        while (readFrame(frame)) {
            received_frames++;
            last_received_frame = frame;
            
            // Create a properly aligned copy of the Ethernet frame
            Ethernet::Frame eth_frame;
            memcpy(&eth_frame, frame.data, sizeof(Ethernet::Frame));
            
            // Log to internal buffer instead of using TEST_LOG
            std::stringstream ss;
            ss << "Received frame: src=" << Ethernet::mac_to_string(eth_frame.src)
               << ", dst=" << Ethernet::mac_to_string(eth_frame.dst)
               << ", prot=" << std::to_string(eth_frame.prot);
            log_messages.push_back(ss.str());
        }
    }

private:
    int signal_count;
    int received_frames;
    SharedFrame last_received_frame;
    std::vector<std::string> log_messages;
};

int main() {
    TEST_INIT("sharedMemoryEngine_test");
    
    // Test 1: Create engines with different IDs
    TEST_LOG("Creating two TestSharedMemoryEngine instances with different IDs");
    TestSharedMemoryEngine engineA(1);
    TestSharedMemoryEngine engineB(2);
    
    TEST_ASSERT(engineA.getId() == 1, "EngineA should have ID 1");
    TEST_ASSERT(engineB.getId() == 2, "EngineB should have ID 2");
    
    // Test 2: Get MAC addresses (should be different based on component ID)
    Ethernet::Address macA = engineA.getMacAddress();
    Ethernet::Address macB = engineB.getMacAddress();
    
    TEST_LOG("EngineA MAC Address: " + Ethernet::mac_to_string(macA));
    TEST_LOG("EngineB MAC Address: " + Ethernet::mac_to_string(macB));
    
    TEST_ASSERT(memcmp(macA.bytes, macB.bytes, Ethernet::MAC_SIZE) != 0, 
                "Engines should have different MAC addresses");
    TEST_ASSERT(macA.bytes[4] == 0 && macA.bytes[5] == 1, 
                "EngineA MAC should encode component ID 1");
    TEST_ASSERT(macB.bytes[4] == 0 && macB.bytes[5] == 2, 
                "EngineB MAC should encode component ID 2");
    
    // Test 3: Engines running status
    TEST_ASSERT(engineA.running() == true, "EngineA should be running after initialization");
    TEST_ASSERT(engineB.running() == true, "EngineB should be running after initialization");
    
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
    
    // Reset counters before sending
    engineA.resetCounters();
    engineB.resetCounters();
    
    TEST_LOG("Sending direct frame with size: " + std::to_string(frame_size) + " bytes");
    
    // Send the frame from EngineA
    int result = engineA.send(frame, frame_size);
    TEST_ASSERT(result >= 0, "Send operation from EngineA should succeed");
    
    // Give some time for the receive thread to process the frame
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Test if engineB received the frame
    TEST_LOG("EngineA Signal count: " + std::to_string(engineA.getSignalCount()));
    TEST_LOG("EngineA Received frames: " + std::to_string(engineA.getReceivedFrames()));
    TEST_LOG("EngineB Signal count: " + std::to_string(engineB.getSignalCount()));
    TEST_LOG("EngineB Received frames: " + std::to_string(engineB.getReceivedFrames()));
    
    TEST_ASSERT(engineB.getSignalCount() > 0, "EngineB should receive a signal");
    TEST_ASSERT(engineB.getReceivedFrames() > 0, "EngineB should receive a frame");
    
    // Print any log messages from engineB
    for (const auto& msg : engineB.getLogMessages()) {
        TEST_LOG("Engine B: " + msg);
    }
    
    // Test 5: Send a broadcast from engineB to all engines
    TEST_LOG("Sending broadcast frame from EngineB");
    
    // Change destination to broadcast
    std::memset(frame->dst.bytes, 0xFF, Ethernet::MAC_SIZE);
    
    // The engine will automatically set the source to engineB's MAC,
    // so we don't need to set it manually here, but we'll do it for clarity
    std::memcpy(frame->src.bytes, macB.bytes, Ethernet::MAC_SIZE);
    
    // Reset counters
    engineA.resetCounters();
    engineB.resetCounters();
    
    // Send from B
    result = engineB.send(frame, frame_size);
    TEST_ASSERT(result >= 0, "Send operation from EngineB should succeed");
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Check results - engineA should receive the broadcast
    TEST_LOG("After broadcast:");
    TEST_LOG("EngineA Signal count: " + std::to_string(engineA.getSignalCount()));
    TEST_LOG("EngineA Received frames: " + std::to_string(engineA.getReceivedFrames()));
    TEST_LOG("EngineB Signal count: " + std::to_string(engineB.getSignalCount()));
    TEST_LOG("EngineB Received frames: " + std::to_string(engineB.getReceivedFrames()));
    
    TEST_ASSERT(engineA.getSignalCount() > 0, "EngineA should receive a broadcast signal");
    TEST_ASSERT(engineA.getReceivedFrames() > 0, "EngineA should receive a broadcast frame");
    
    // Print any log messages from engineA
    for (const auto& msg : engineA.getLogMessages()) {
        TEST_LOG("Engine A: " + msg);
    }
    
    // Get the received frame data
    if (engineA.getReceivedFrames() > 0) {
        // Get the last received frame data
        SharedMemoryEngine::SharedFrame last_frame = engineA.getLastReceivedFrame();
        
        // Use memcpy to get a properly aligned copy of the Ethernet frame
        Ethernet::Frame received_frame;
        memcpy(&received_frame, last_frame.data, sizeof(Ethernet::Frame));
        
        // Verify the frame protocol and source
        TEST_ASSERT(received_frame.prot == 0x8888, "Received frame should have correct protocol");
        
        // Compare the source MAC with engineB's actual MAC, not what we set manually
        // The address should always be the actual address of the sending component
        TEST_LOG("Comparing source address: " + Ethernet::mac_to_string(received_frame.src) + 
                 " with expected: " + Ethernet::mac_to_string(macB));
        TEST_ASSERT(memcmp(received_frame.src.bytes, macB.bytes, Ethernet::MAC_SIZE) == 0, 
                    "Received frame should have the sending engine's MAC as source address");
    }
    
    // Test 6: Stop the engines
    TEST_LOG("Stopping engines");
    engineA.stop();
    engineB.stop();
    
    TEST_ASSERT(engineA.running() == false, "EngineA should not be running after stop");
    TEST_ASSERT(engineB.running() == false, "EngineB should not be running after stop");
    
    // Clean up
    delete[] buffer;
    
    std::cout << "SharedMemoryEngine test completed successfully!" << std::endl;
    return 0;
} 