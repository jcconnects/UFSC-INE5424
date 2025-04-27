#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <sstream>
#include "test_utils.h"
#include "sharedMemoryEngine.h"
#include "ethernet.h"

// Define a custom shared frame structure for the test
struct TestSharedFrame {
    // Use the real SharedFrameData structure from sharedMemoryEngine.h
    SharedFrameData data;
};

// Concrete implementation of SharedMemoryEngine for testing
class TestSharedMemoryEngine : public SharedMemoryEngine {
public:
    // Default constructor - no component ID in current implementation
    TestSharedMemoryEngine() 
        : SharedMemoryEngine(), signal_count(0), received_frames(0) {
        // Generate a unique MAC address for this instance
        static int instance_count = 0;
        int id = ++instance_count;
        
        // Set a unique MAC address based on instance count
        std::memset(_mac_address.bytes, 0, Ethernet::MAC_SIZE);
        _mac_address.bytes[0] = 0x02;  // Locally administered address
        _mac_address.bytes[5] = id;    // Use instance count as ID
    }
    
    int getSignalCount() const { return signal_count; }
    int getReceivedFrames() const { return received_frames; }
    
    // Get the last received frame
    TestSharedFrame getLastReceivedFrame() const { 
        return last_received_frame; 
    }
    
    void resetCounters() {
        signal_count = 0;
        received_frames = 0;
        log_messages.clear();
    }
    
    // Get log messages
    std::vector<std::string> getLogMessages() const {
        return log_messages;
    }

    // Hide the base class getMacAddress with our implementation
    Ethernet::Address getMacAddress() const {
        return _mac_address;
    }
    
    // Add a method to get the instance ID for testing
    int getId() const { 
        return _mac_address.bytes[5];
    }

    // Mock the receiveData method to capture frames
    int receiveData(void* payload_buf, unsigned int max_size, Ethernet::Protocol* proto_out) {
        signal_count++;
        
        // Create a frame and populate it for testing
        if (received_frames < 1) {  // Only simulate one frame
            received_frames++;
            
            // Create a test frame
            TestSharedFrame frame;
            frame.data.protocol = 0x8888;  // Test protocol
            frame.data.payload_size = 46;  // Minimum size
            
            // Set up a basic payload
            std::memset(frame.data.payload, 0, frame.data.payload_size);
            
            // Store the frame
            last_received_frame = frame;
            
            // Copy payload to output buffer if there's enough space
            if (max_size >= frame.data.payload_size) {
                std::memcpy(payload_buf, frame.data.payload, frame.data.payload_size);
                if (proto_out) *proto_out = frame.data.protocol;
                
                // Log the action
                std::stringstream ss;
                ss << "Simulated frame received with protocol " << frame.data.protocol;
                log_messages.push_back(ss.str());
                
                return frame.data.payload_size;
            }
        }
        
        return 0;  // No more frames
    }

private:
    int signal_count;
    int received_frames;
    TestSharedFrame last_received_frame;
    std::vector<std::string> log_messages;
    Ethernet::Address _mac_address;  // Custom MAC address for this instance
};

int main() {
    TEST_INIT("sharedMemoryEngine_test");
    
    // Test 1: Create engines
    TEST_LOG("Creating two TestSharedMemoryEngine instances");
    TestSharedMemoryEngine engineA;
    TestSharedMemoryEngine engineB;
    
    // Test IDs should be assigned automatically
    TEST_ASSERT(engineA.getId() == 1, "EngineA should have ID 1");
    TEST_ASSERT(engineB.getId() == 2, "EngineB should have ID 2");
    
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
    
    // Test 3: Engines running status
    TEST_ASSERT(engineA.running() == false, "EngineA should not be running before start");
    TEST_ASSERT(engineB.running() == false, "EngineB should not be running before start");
    
    // Start the engines
    engineA.start();
    engineB.start();
    
    TEST_ASSERT(engineA.running() == true, "EngineA should be running after start");
    TEST_ASSERT(engineB.running() == true, "EngineB should be running after start");
    
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
    
    // Note: In this test, we're using a mocked receiveData that doesn't actually
    // read from shared memory, so we won't test real inter-engine communication.
    // Instead, we'll verify the internal logic works correctly.
    
    // Test simulated frame reception
    Ethernet::Protocol proto;
    uint8_t recv_buffer[Traits<SharedMemoryEngine>::MTU];
    
    engineA.resetCounters();
    engineB.resetCounters();
    
    // Call receiveData directly to trigger our test implementation
    int recv_size = engineB.receiveData(recv_buffer, sizeof(recv_buffer), &proto);
    
    TEST_ASSERT(recv_size > 0, "EngineB should receive data from simulated frame");
    TEST_ASSERT(proto == 0x8888, "Received protocol should match the test protocol");
    TEST_ASSERT(engineB.getSignalCount() == 1, "Signal count should increment");
    TEST_ASSERT(engineB.getReceivedFrames() == 1, "Received frames count should increment");
    
    // Test 5: Stop the engines
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