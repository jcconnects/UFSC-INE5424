#define TEST_MODE 1

#include <iostream>
#include <string>
#include <cassert>
#include <cstring>
#include <thread>
#include <chrono>
#include <vector>
#include <mutex>
#include <condition_variable>
#include "../../include/protocol.h"
#include "../../include/nic.h"
#include "../../include/socketEngine.h"
#include "../../include/ethernet.h"
#include "../../include/traits.h"
#include "../../include/debug.h"
#include "test_utils.h"

// Helper class for testing the Protocol implementation
class ProtocolTestEngine : public SocketEngine {
public:
    ProtocolTestEngine() : SocketEngine() {}
    virtual ~ProtocolTestEngine() = default;
    
protected:
    // Implementation of the pure virtual method required by SocketEngine
    void handleSignal() override {
        // Implementation not needed for this test
    }
};

// Define required trait specialization for our test
// Note: This must be a template specialization, not a namespace
template<> struct Traits<Protocol<NIC<ProtocolTestEngine>>> : public Traits<void> {
    static const bool debugged = false;  // Turn off debugging for Protocol
    static const unsigned int ETHERNET_PROTOCOL_NUMBER = 0x1234;
};

// Turn off debugging for NIC<ProtocolTestEngine>
template<> struct Traits<NIC<ProtocolTestEngine>> : public Traits<void> {
    static const bool debugged = false;
    static const unsigned int SEND_BUFFERS = 16;
    static const unsigned int RECEIVE_BUFFERS = 16;
};

// Test class that will serve as an observer for Protocol
template <typename P>
class ProtocolObserver : public P::Observer {
public:
    ProtocolObserver(typename P::Port port) 
        : P::Observer(port), 
          received_count(0), 
          last_buffer(nullptr),
          last_size(0),
          data_received(false) {}
    
    ~ProtocolObserver() override = default;
    
    // Match the signature from the parent class (Conditional_Data_Observer)
    void update(typename P::Port condition, typename P::Buffer* buf) override {
        std::lock_guard<std::mutex> lock(mtx);
        received_count++;
        last_port = condition;
        last_buffer = buf;
        
        // Copy data from buffer for testing
        last_size = buf->size();
        data_received = true;
        cv.notify_one();
    }
    
    bool waitForData(int timeout_ms = 5000) {
        std::unique_lock<std::mutex> lock(mtx);
        return cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), 
                          [this]{ return data_received; });
    }
    
    void resetData() {
        std::lock_guard<std::mutex> lock(mtx);
        data_received = false;
        last_buffer = nullptr;
        last_size = 0;
    }
    
    int received_count;
    typename P::Port last_port;
    typename P::Buffer* last_buffer;
    unsigned int last_size;
    bool data_received;
    
private:
    std::mutex mtx;
    std::condition_variable cv;
};

class Initializer {
public:
    typedef NIC<ProtocolTestEngine> NICType;
    typedef Protocol<NICType> ProtocolType;

    static NICType* create_nic(unsigned int id) {
        // Setting virtual MAC Address
        Ethernet::Address addr;
        addr.bytes[0] = 0x02; // local, unicast
        addr.bytes[1] = 0x00;
        addr.bytes[2] = 0x00;
        addr.bytes[3] = 0x00;
        addr.bytes[4] = (id >> 8) & 0xFF;
        addr.bytes[5] = id & 0xFF;

        NICType* nic = new NICType();
        nic->setAddress(addr);
        return nic;
    }
    
    static ProtocolType* create_protocol(NICType* nic) {
        return new ProtocolType(nic);
    }
};

int main() {
    TEST_INIT("protocol_test");
    
    // Create NIC instances for testing
    TEST_LOG("Creating NIC instances");
    auto nic1 = Initializer::create_nic(1);
    auto nic2 = Initializer::create_nic(2);
    
    // Create Protocol instances
    TEST_LOG("Creating Protocol instances");
    typedef Initializer::ProtocolType ProtocolImpl;
    auto proto1 = Initializer::create_protocol(nic1);
    auto proto2 = Initializer::create_protocol(nic2);
    
    // Test 1: Protocol::Address class
    TEST_LOG("Testing Protocol::Address class");
    
    // Test Address default constructor
    auto nullAddr = ProtocolImpl::Address();
    TEST_ASSERT(nullAddr.port() == 0, "Default address port should be 0");
    TEST_ASSERT(nullAddr.paddr() == Ethernet::NULL_ADDRESS, "Default address paddr should be NULL_ADDRESS");
    TEST_ASSERT(!nullAddr, "Default address should evaluate to false");
    
    // Test Address constructor with values
    Ethernet::Address mac1 = nic1->address();
    auto port1 = static_cast<ProtocolImpl::Port>(1234);
    auto addr1 = ProtocolImpl::Address(mac1, port1);
    
    TEST_ASSERT(addr1.port() == port1, "Address port should match the value set");
    TEST_ASSERT(addr1.paddr() == mac1, "Address paddr should match the value set");
    TEST_ASSERT(addr1, "Non-null address should evaluate to true");
    
    // Test Address equality
    auto addr2 = ProtocolImpl::Address(mac1, port1);
    TEST_ASSERT(addr1 == addr2, "Identical addresses should be equal");
    
    auto addr3 = ProtocolImpl::Address(mac1, port1 + 1);
    TEST_ASSERT(!(addr1 == addr3), "Addresses with different ports should not be equal");
    
    // Test 2: Observer pattern
    TEST_LOG("Testing Protocol observer pattern");
    
    // Create observers for both protocols
    auto observer1 = new ProtocolObserver<ProtocolImpl>(port1);
    
    // Test attach observer
    ProtocolImpl::attach(observer1, addr1);
    TEST_LOG("Observer attached to port " + std::to_string(port1));
    
    // Test 3: Send and receive functionality
    TEST_LOG("Testing send and receive functionality");
    
    // Prepare test data
    const char* test_message = "Hello Protocol World!";
    size_t msg_len = strlen(test_message) + 1; // Include null terminator
    
    // Define addresses for communication
    Ethernet::Address mac2 = nic2->address();
    auto port2 = static_cast<ProtocolImpl::Port>(5678);
    auto src_addr = ProtocolImpl::Address(mac1, port1);
    auto dst_addr = ProtocolImpl::Address(mac2, port2);
    
    // Test send functionality
    TEST_LOG("Sending message from proto1 to proto2");
    int bytes_sent = proto1->send(src_addr, dst_addr, test_message, msg_len);
    TEST_ASSERT(bytes_sent > 0, "Send should return a positive number of bytes");
    TEST_LOG("Sent " + std::to_string(bytes_sent) + " bytes");
    
    // Give some time for message processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Create and attach observer for receiving on proto2
    auto observer2 = new ProtocolObserver<ProtocolImpl>(port2);
    ProtocolImpl::attach(observer2, dst_addr);
    TEST_LOG("Observer attached to proto2 with port " + std::to_string(port2));
    
    // Send another message after observer is attached
    TEST_LOG("Sending second message from proto1 to proto2");
    bytes_sent = proto1->send(src_addr, dst_addr, test_message, msg_len);
    TEST_ASSERT(bytes_sent > 0, "Send should return a positive number of bytes");
    
    // Wait for the message to be received
    TEST_LOG("Waiting for message to be received");
    bool received = observer2->waitForData();
    TEST_ASSERT(received, "Message should be received within timeout period");
    
    if (received) {
        TEST_LOG("Message received, checking buffer");
        TEST_ASSERT(observer2->last_buffer != nullptr, "Received buffer should not be null");
        TEST_ASSERT(observer2->last_port == port2, "Received port should match destination port");
        
        // Test 4: Receive functionality
        char received_data[100] = {0};
        int bytes_received = proto2->receive(observer2->last_buffer, src_addr, received_data, sizeof(received_data));
        
        TEST_ASSERT(bytes_received > 0, "Receive should return a positive number of bytes");
        TEST_LOG("Received " + std::to_string(bytes_received) + " bytes");
        TEST_ASSERT(strcmp(received_data, test_message) == 0, "Received message should match sent message");
        TEST_LOG("Received message: " + std::string(received_data));
        
        // Verify source address was properly set
        TEST_ASSERT(src_addr.port() == port1, "Received source port should match sender port");
        TEST_ASSERT(src_addr.paddr() == mac1, "Received source MAC should match sender MAC");
    }
    
    // Test 5: Test observer detach
    TEST_LOG("Testing observer detach");
    ProtocolImpl::detach(observer2, dst_addr);
    observer2->resetData();
    
    // Send message after detach, should not be received by observer
    TEST_LOG("Sending message after observer detach");
    bytes_sent = proto1->send(src_addr, dst_addr, test_message, msg_len);
    TEST_ASSERT(bytes_sent > 0, "Send should still return a positive number of bytes");
    
    // Wait a bit to ensure message had time to process
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Verify observer did not receive the message
    TEST_ASSERT(!observer2->data_received, "Observer should not receive message after detach");
    
    // Test 6: Test large data handling
    TEST_LOG("Testing large data handling");
    const size_t large_size = ProtocolImpl::MTU - 10; // Just under MTU limit
    std::vector<char> large_data(large_size);
    
    // Fill with sequential data
    for (size_t i = 0; i < large_size; i++) {
        large_data[i] = static_cast<char>(i % 256);
    }
    
    // Re-attach observer
    ProtocolImpl::attach(observer2, dst_addr);
    observer2->resetData();
    
    // Send large data
    TEST_LOG("Sending large data (" + std::to_string(large_size) + " bytes)");
    bytes_sent = proto1->send(src_addr, dst_addr, large_data.data(), large_size);
    TEST_ASSERT(bytes_sent > 0, "Send should return a positive number of bytes for large data");
    
    // Wait for the message
    received = observer2->waitForData();
    TEST_ASSERT(received, "Large message should be received within timeout period");
    
    if (received) {
        std::vector<char> received_large(large_size);
        int bytes_received = proto2->receive(observer2->last_buffer, src_addr, received_large.data(), received_large.size());
        
        TEST_ASSERT(bytes_received > 0, "Receive should return a positive number of bytes for large data");
        TEST_ASSERT(static_cast<size_t>(bytes_received) <= large_size, "Received bytes should not exceed sent bytes");
        
        // Verify large data integrity
        bool data_intact = true;
        for (int i = 0; i < bytes_received; i++) {
            if (received_large[i] != static_cast<char>(i % 256)) {
                data_intact = false;
                break;
            }
        }
        TEST_ASSERT(data_intact, "Large data should be received intact");
    }
    
    // Test 7: Test BROADCAST address
    TEST_LOG("Testing BROADCAST address");
    auto broadcast_addr = ProtocolImpl::Address::BROADCAST;
    TEST_ASSERT(memcmp(broadcast_addr.paddr().bytes, "\xFF\xFF\xFF\xFF\xFF\xFF", 6) == 0, 
                "BROADCAST address should have broadcast MAC (FF:FF:FF:FF:FF:FF)");
    
    // Clean up
    TEST_LOG("Cleaning up");
    ProtocolImpl::detach(observer1, addr1);
    ProtocolImpl::detach(observer2, dst_addr);
    
    delete observer1;
    delete observer2;
    delete proto1;
    delete proto2;
    delete nic1;
    delete nic2;
    
    TEST_LOG("Protocol test passed successfully!");
    return 0;
} 