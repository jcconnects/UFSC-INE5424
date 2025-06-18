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
#include <atomic>

#include "api/network/protocol.h"
#include "api/network/nic.h"
#include "api/network/socketEngine.h"
#include "api/network/ethernet.h"
#include "api/network/initializer.h"
#include "api/traits.h"
#include "../testcase.h"

using namespace std::chrono_literals;

// Forward declarations
class ProtocolTest;
class ProtocolInitializer;

/**
 * @brief Helper class for Protocol initialization and management
 * 
 * Provides factory methods and utilities for creating and configuring
 * Protocol instances for testing purposes. Encapsulates the initialization
 * logic to ensure consistent test setup across different test methods.
 */
class ProtocolInitializer : public Initializer {
public:
    typedef NIC<SocketEngine> NICType;
    typedef Protocol<NICType> ProtocolType;

    ProtocolInitializer() = default;
    ~ProtocolInitializer() = default;

    /**
     * @brief Creates a NIC instance with specified vehicle ID
     * 
     * @param id Vehicle ID to use for MAC address generation
     * @return Pointer to newly created NIC instance
     * 
     * Creates a NIC instance with a virtual MAC address based on the
     * provided vehicle ID. The MAC address follows the pattern:
     * 02:00:00:00:XX:XX where XX:XX represents the vehicle ID.
     */
    static NICType* create_test_nic(unsigned int id);
    
    /**
     * @brief Creates a Protocol instance with specified NIC
     * 
     * @param nic Pointer to the NIC instance to use
     * @return Pointer to newly created Protocol instance
     */
    static ProtocolType* create_test_protocol(NICType* nic);

    /**
     * @brief Creates a test Ethernet address with specified ID
     * 
     * @param id ID to embed in the MAC address
     * @return Ethernet address with the specified ID
     * 
     * Generates a standardized test MAC address for consistent
     * testing across different test methods.
     */
    static Ethernet::Address create_test_address(unsigned int id);
};

/**
 * @brief Test observer class for Protocol testing
 * 
 * Implements the observer pattern to monitor Protocol events and data
 * reception. Provides thread-safe mechanisms for waiting and verifying
 * received data during testing.
 */
template <typename P>
class ProtocolObserver : public P::Observer {
public:
    /**
     * @brief Constructor for ProtocolObserver
     * 
     * @param port Port number to observe
     */
    ProtocolObserver(typename P::Port port) 
        : P::Observer(port), 
          received_count(0), 
          last_buffer(nullptr),
          last_size(0),
          data_received(false) {}
    
    ~ProtocolObserver() override = default;
    
    /**
     * @brief Update method called when data is received
     * 
     * @param condition Port condition that triggered the update
     * @param buf Buffer containing the received data
     */
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
    
    /**
     * @brief Wait for data to be received with timeout
     * 
     * @param timeout_ms Timeout in milliseconds
     * @return true if data was received within timeout, false otherwise
     */
    bool waitForData(int timeout_ms = 5000) {
        std::unique_lock<std::mutex> lock(mtx);
        return cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), 
                          [this]{ return data_received; });
    }
    
    /**
     * @brief Reset the observer state for new test
     */
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

/**
 * @brief Comprehensive test suite for Protocol functionality
 * 
 * Tests all aspects of Protocol operation including address management,
 * observer pattern implementation, send/receive functionality, and
 * error handling. Organized into logical test groups for better
 * maintainability and clarity.
 */
class ProtocolTest : public TestCase {
protected:
    void setUp() override;
    void tearDown() override;

    // Helper methods
    void cleanupResources();

    // === ADDRESS MANAGEMENT TESTS ===
    void testProtocolAddressDefaultConstructor();
    void testProtocolAddressConstructorWithValues();
    void testProtocolAddressEquality();
    void testProtocolAddressBroadcast();

    // === OBSERVER PATTERN TESTS ===
    void testObserverAttachAndDetach();
    void testObserverNotificationOnReceive();
    void testMultipleObserversOnSamePort();
    void testObserverDetachStopsNotifications();

    // === SEND AND RECEIVE TESTS ===
    void testBasicSendAndReceive();
    void testSendToNonExistentReceiver();
    void testReceiveWithValidBuffer();
    void testReceiveWithInvalidBuffer();

    // === LARGE DATA HANDLING TESTS ===
    void testLargeDataTransmission();
    void testDataIntegrityVerification();
    void testMTULimitHandling();

    // === ERROR HANDLING TESTS ===
    void testSendWithNullData();
    void testSendWithZeroSize();
    void testReceiveWithNullBuffer();

    // === THREAD SAFETY TESTS ===
    void testConcurrentSendOperations();
    void testConcurrentObserverOperations();
    void testConcurrentSendReceiveOperations();

public:
    ProtocolTest();

private:
    ProtocolInitializer::NICType* nic1;
    ProtocolInitializer::NICType* nic2;
    ProtocolInitializer::ProtocolType* proto1;
    ProtocolInitializer::ProtocolType* proto2;
    std::vector<ProtocolObserver<ProtocolInitializer::ProtocolType>*> observers;
};

/**
 * @brief Constructor that registers all test methods
 * 
 * Organizes tests into logical groups for better maintainability and clarity.
 * Each test method name clearly describes what functionality is being tested.
 */
ProtocolTest::ProtocolTest() : nic1(nullptr), nic2(nullptr), proto1(nullptr), proto2(nullptr) {
    // === ADDRESS MANAGEMENT TESTS ===
    DEFINE_TEST(testProtocolAddressDefaultConstructor);
    DEFINE_TEST(testProtocolAddressConstructorWithValues);
    DEFINE_TEST(testProtocolAddressEquality);
    DEFINE_TEST(testProtocolAddressBroadcast);

    // === OBSERVER PATTERN TESTS ===
    DEFINE_TEST(testObserverAttachAndDetach);
    DEFINE_TEST(testObserverNotificationOnReceive);
    DEFINE_TEST(testMultipleObserversOnSamePort);
    // DEFINE_TEST(testObserverDetachStopsNotifications);

    // === SEND AND RECEIVE TESTS ===
    // DEFINE_TEST(testBasicSendAndReceive);
    // DEFINE_TEST(testSendToNonExistentReceiver);
    // DEFINE_TEST(testReceiveWithValidBuffer);
    // DEFINE_TEST(testReceiveWithInvalidBuffer);

    // === LARGE DATA HANDLING TESTS ===
    // DEFINE_TEST(testLargeDataTransmission);
    // DEFINE_TEST(testDataIntegrityVerification);
    // DEFINE_TEST(testMTULimitHandling);

    // === ERROR HANDLING TESTS ===
    // DEFINE_TEST(testSendWithNullData);
    // DEFINE_TEST(testSendWithZeroSize);
    // DEFINE_TEST(testReceiveWithNullBuffer);

    // === THREAD SAFETY TESTS ===
    // DEFINE_TEST(testConcurrentSendOperations);
    // DEFINE_TEST(testConcurrentObserverOperations);
    // DEFINE_TEST(testConcurrentSendReceiveOperations);
}

void ProtocolTest::setUp() {
    // Create NIC instances for testing
    nic1 = ProtocolInitializer::create_test_nic(1);
    nic2 = ProtocolInitializer::create_test_nic(2);
    
    // Create Protocol instances
    proto1 = ProtocolInitializer::create_test_protocol(nic1);
    proto2 = ProtocolInitializer::create_test_protocol(nic2);
}

void ProtocolTest::tearDown() {
    cleanupResources();
}

void ProtocolTest::cleanupResources() {
    // Clean up observers
    for (auto observer : observers) {
        delete observer;
    }
    observers.clear();
    
    // Clean up protocols
    if (proto1) {
        delete proto1;
        proto1 = nullptr;
    }
    if (proto2) {
        delete proto2;
        proto2 = nullptr;
    }
    
    // Clean up NICs
    if (nic1) {
        delete nic1;
        nic1 = nullptr;
    }
    if (nic2) {
        delete nic2;
        nic2 = nullptr;
    }
}

// === ADDRESS MANAGEMENT TESTS ===

void ProtocolTest::testProtocolAddressDefaultConstructor() {
    typedef ProtocolInitializer::ProtocolType ProtocolImpl;
    
    // Test Address default constructor
    auto nullAddr = ProtocolImpl::Address();
    assert_true(nullAddr.port() == 0, "Default address port should be 0");
    assert_true(nullAddr.paddr() == Ethernet::NULL_ADDRESS, "Default address paddr should be NULL_ADDRESS");
    assert_false(nullAddr, "Default address should evaluate to false");
}

void ProtocolTest::testProtocolAddressConstructorWithValues() {
    typedef ProtocolInitializer::ProtocolType ProtocolImpl;
    
    // Test Address constructor with values
    Ethernet::Address mac1 = nic1->address();
    auto port1 = static_cast<ProtocolImpl::Port>(1234);
    auto addr1 = ProtocolImpl::Address(mac1, port1);
    
    assert_true(addr1.port() == port1, "Address port should match the value set");
    assert_true(addr1.paddr() == mac1, "Address paddr should match the value set");
    assert_true(addr1, "Non-null address should evaluate to true");
}

void ProtocolTest::testProtocolAddressEquality() {
    typedef ProtocolInitializer::ProtocolType ProtocolImpl;
    
    Ethernet::Address mac1 = nic1->address();
    auto port1 = static_cast<ProtocolImpl::Port>(1234);
    
    // Test Address equality
    auto addr1 = ProtocolImpl::Address(mac1, port1);
    auto addr2 = ProtocolImpl::Address(mac1, port1);
    assert_true(addr1 == addr2, "Identical addresses should be equal");
    
    auto addr3 = ProtocolImpl::Address(mac1, port1 + 1);
    assert_false(addr1 == addr3, "Addresses with different ports should not be equal");
}

void ProtocolTest::testProtocolAddressBroadcast() {
    typedef ProtocolInitializer::ProtocolType ProtocolImpl;
    
    // Test BROADCAST address
    auto broadcast_addr = ProtocolImpl::Address::BROADCAST;
    assert_true(memcmp(broadcast_addr.paddr().bytes, "\xFF\xFF\xFF\xFF\xFF\xFF", 6) == 0, 
                "BROADCAST address should have broadcast MAC (FF:FF:FF:FF:FF:FF)");
}

// === OBSERVER PATTERN TESTS ===

void ProtocolTest::testObserverAttachAndDetach() {
    typedef ProtocolInitializer::ProtocolType ProtocolImpl;
    
    Ethernet::Address mac1 = nic1->address();
    auto port1 = static_cast<ProtocolImpl::Port>(1234);
    auto addr1 = ProtocolImpl::Address(mac1, port1);
    
    // Create observer
    auto observer = new ProtocolObserver<ProtocolImpl>(port1);
    observers.push_back(observer);
    
    // Test attach observer
    ProtocolImpl::attach(observer, addr1);
    
    // Test detach observer
    ProtocolImpl::detach(observer, addr1);
}

void ProtocolTest::testObserverNotificationOnReceive() {
    typedef ProtocolInitializer::ProtocolType ProtocolImpl;
    
    // Define addresses for communication
    Ethernet::Address mac1 = nic1->address();
    Ethernet::Address mac2 = nic2->address();
    auto port1 = static_cast<ProtocolImpl::Port>(1234);
    auto port2 = static_cast<ProtocolImpl::Port>(5678);
    auto src_addr = ProtocolImpl::Address(mac1, port1);
    auto dst_addr = ProtocolImpl::Address(mac2, port2);
    
    // Create and attach observer for receiving
    auto observer = new ProtocolObserver<ProtocolImpl>(port2);
    observers.push_back(observer);
    ProtocolImpl::attach(observer, dst_addr);
    
    // Prepare test data
    const char* test_message = "Hello Protocol World!";
    size_t msg_len = strlen(test_message) + 1;
    
    // Send message
    int bytes_sent = proto1->send(src_addr, dst_addr, test_message, msg_len);
    assert_true(bytes_sent > 0, "Send should return a positive number of bytes");
    
    // Wait for the message to be received
    bool received = observer->waitForData();
    assert_true(received, "Message should be received within timeout period");
    
    if (received) {
        assert_true(observer->last_buffer != nullptr, "Received buffer should not be null");
        assert_true(observer->last_port == port2, "Received port should match destination port");
    }
}

void ProtocolTest::testMultipleObserversOnSamePort() {
    typedef ProtocolInitializer::ProtocolType ProtocolImpl;
    
    Ethernet::Address mac2 = nic2->address();
    auto port2 = static_cast<ProtocolImpl::Port>(5678);
    auto dst_addr = ProtocolImpl::Address(mac2, port2);
    
    // Create multiple observers for the same port
    auto observer1 = new ProtocolObserver<ProtocolImpl>(port2);
    auto observer2 = new ProtocolObserver<ProtocolImpl>(port2);
    observers.push_back(observer1);
    observers.push_back(observer2);
    
    // Attach both observers
    ProtocolImpl::attach(observer1, dst_addr);
    ProtocolImpl::attach(observer2, dst_addr);
    
    // Send a message
    const char* test_message = "Test message";
    size_t msg_len = strlen(test_message) + 1;
    Ethernet::Address mac1 = nic1->address();
    auto port1 = static_cast<ProtocolImpl::Port>(1234);
    auto src_addr = ProtocolImpl::Address(mac1, port1);
    
    int bytes_sent = proto1->send(src_addr, dst_addr, test_message, msg_len);
    assert_true(bytes_sent > 0, "Send should return a positive number of bytes");
    
    // Both observers should receive the message
    bool received1 = observer1->waitForData();
    bool received2 = observer2->waitForData();
    
    assert_true(received1, "First observer should receive the message");
    assert_true(received2, "Second observer should receive the message");
}

void ProtocolTest::testObserverDetachStopsNotifications() {
    typedef ProtocolInitializer::ProtocolType ProtocolImpl;
    
    Ethernet::Address mac1 = nic1->address();
    Ethernet::Address mac2 = nic2->address();
    auto port1 = static_cast<ProtocolImpl::Port>(1234);
    auto port2 = static_cast<ProtocolImpl::Port>(5678);
    auto src_addr = ProtocolImpl::Address(mac1, port1);
    auto dst_addr = ProtocolImpl::Address(mac2, port2);
    
    // Create and attach observer
    auto observer = new ProtocolObserver<ProtocolImpl>(port2);
    observers.push_back(observer);
    ProtocolImpl::attach(observer, dst_addr);
    
    // Detach observer
    ProtocolImpl::detach(observer, dst_addr);
    observer->resetData();
    
    // Send message after detach
    const char* test_message = "Test message";
    size_t msg_len = strlen(test_message) + 1;
    std::cout << "Sending message after detach" << std::endl;
    int bytes_sent = proto1->send(src_addr, dst_addr, test_message, msg_len);
    std::cout << "Bytes sent: " << bytes_sent << std::endl;
    assert_true(bytes_sent > 0, "Send should still return a positive number of bytes");
    
    // Wait a bit to ensure message had time to process
    std::cout << "Waiting for message to be processed" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Verify observer did not receive the message
    std::cout << "Verifying observer did not receive the message" << std::endl;
    assert_false(observer->data_received, "Observer should not receive message after detach");
}

// === SEND AND RECEIVE TESTS ===

void ProtocolTest::testBasicSendAndReceive() {
    typedef ProtocolInitializer::ProtocolType ProtocolImpl;
    
    // Define addresses for communication
    Ethernet::Address mac1 = nic1->address();
    Ethernet::Address mac2 = nic2->address();
    auto port1 = static_cast<ProtocolImpl::Port>(1234);
    auto port2 = static_cast<ProtocolImpl::Port>(5678);
    auto src_addr = ProtocolImpl::Address(mac1, port1);
    auto dst_addr = ProtocolImpl::Address(mac2, port2);
    
    // Create and attach observer for receiving
    auto observer = new ProtocolObserver<ProtocolImpl>(port2);
    observers.push_back(observer);
    ProtocolImpl::attach(observer, dst_addr);
    
    // Prepare test data
    const char* test_message = "Hello Protocol World!";
    size_t msg_len = strlen(test_message) + 1;
    
    // Send message
    int bytes_sent = proto1->send(src_addr, dst_addr, test_message, msg_len);
    assert_true(bytes_sent > 0, "Send should return a positive number of bytes");
    
    // Wait for the message to be received
    bool received = observer->waitForData();
    assert_true(received, "Message should be received within timeout period");
    
    if (received) {
        // Test receive functionality
        char received_data[100] = {0};
        int bytes_received = proto2->receive(observer->last_buffer, &src_addr, received_data, sizeof(received_data));
        
        assert_true(bytes_received > 0, "Receive should return a positive number of bytes");
        assert_true(strcmp(received_data, test_message) == 0, "Received message should match sent message");
        
        // Verify source address was properly set
        assert_true(src_addr.port() == port1, "Received source port should match sender port");
        assert_true(src_addr.paddr() == mac1, "Received source MAC should match sender MAC");
    }
}

void ProtocolTest::testSendToNonExistentReceiver() {
    typedef ProtocolInitializer::ProtocolType ProtocolImpl;
    
    // Define addresses for communication to non-existent receiver
    Ethernet::Address mac1 = nic1->address();
    Ethernet::Address fake_mac = {{0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00}};
    auto port1 = static_cast<ProtocolImpl::Port>(1234);
    auto port2 = static_cast<ProtocolImpl::Port>(5678);
    auto src_addr = ProtocolImpl::Address(mac1, port1);
    auto dst_addr = ProtocolImpl::Address(fake_mac, port2);
    
    // Prepare test data
    const char* test_message = "Hello Non-existent!";
    size_t msg_len = strlen(test_message) + 1;
    
    // Send message to non-existent receiver
    int bytes_sent = proto1->send(src_addr, dst_addr, test_message, msg_len);
    assert_true(bytes_sent > 0, "Send should still return a positive number of bytes even for non-existent receiver");
}

void ProtocolTest::testReceiveWithValidBuffer() {
    typedef ProtocolInitializer::ProtocolType ProtocolImpl;
    
    // This test is covered in testBasicSendAndReceive
    // Additional specific buffer validation tests can be added here
}

void ProtocolTest::testReceiveWithInvalidBuffer() {
    typedef ProtocolInitializer::ProtocolType ProtocolImpl;
    
    // Test receive with null buffer
    Ethernet::Address mac1 = nic1->address();
    auto port1 = static_cast<ProtocolImpl::Port>(1234);
    auto src_addr = ProtocolImpl::Address(mac1, port1);
    char received_data[100] = {0};
    
    // This should handle null buffer gracefully
    int bytes_received = proto2->receive(nullptr, &src_addr, received_data, sizeof(received_data));
    assert_true(bytes_received <= 0, "Receive with null buffer should return 0 or negative value");
}

// === LARGE DATA HANDLING TESTS ===

void ProtocolTest::testLargeDataTransmission() {
    typedef ProtocolInitializer::ProtocolType ProtocolImpl;
    
    // Define addresses for communication
    Ethernet::Address mac1 = nic1->address();
    Ethernet::Address mac2 = nic2->address();
    auto port1 = static_cast<ProtocolImpl::Port>(1234);
    auto port2 = static_cast<ProtocolImpl::Port>(5678);
    auto src_addr = ProtocolImpl::Address(mac1, port1);
    auto dst_addr = ProtocolImpl::Address(mac2, port2);
    
    // Create and attach observer
    auto observer = new ProtocolObserver<ProtocolImpl>(port2);
    observers.push_back(observer);
    ProtocolImpl::attach(observer, dst_addr);
    
    // Test large data handling
    const size_t large_size = ProtocolImpl::MTU - 10; // Just under MTU limit
    std::vector<char> large_data(large_size);
    
    // Fill with sequential data
    for (size_t i = 0; i < large_size; i++) {
        large_data[i] = static_cast<char>(i % 256);
    }
    
    // Send large data
    int bytes_sent = proto1->send(src_addr, dst_addr, large_data.data(), large_size);
    assert_true(bytes_sent > 0, "Send should return a positive number of bytes for large data");
    
    // Wait for the message
    bool received = observer->waitForData();
    assert_true(received, "Large message should be received within timeout period");
}

void ProtocolTest::testDataIntegrityVerification() {
    typedef ProtocolInitializer::ProtocolType ProtocolImpl;
    
    // Define addresses for communication
    Ethernet::Address mac1 = nic1->address();
    Ethernet::Address mac2 = nic2->address();
    auto port1 = static_cast<ProtocolImpl::Port>(1234);
    auto port2 = static_cast<ProtocolImpl::Port>(5678);
    auto src_addr = ProtocolImpl::Address(mac1, port1);
    auto dst_addr = ProtocolImpl::Address(mac2, port2);
    
    // Create and attach observer
    auto observer = new ProtocolObserver<ProtocolImpl>(port2);
    observers.push_back(observer);
    ProtocolImpl::attach(observer, dst_addr);
    
    const size_t test_size = 1000;
    std::vector<char> test_data(test_size);
    
    // Fill with sequential data
    for (size_t i = 0; i < test_size; i++) {
        test_data[i] = static_cast<char>(i % 256);
    }
    
    // Send data
    int bytes_sent = proto1->send(src_addr, dst_addr, test_data.data(), test_size);
    assert_true(bytes_sent > 0, "Send should return a positive number of bytes");
    
    // Wait for the message
    bool received = observer->waitForData();
    assert_true(received, "Message should be received within timeout period");
    
    if (received) {
        std::vector<char> received_data(test_size);
        int bytes_received = proto2->receive(observer->last_buffer, &src_addr, received_data.data(), received_data.size());
        
        assert_true(bytes_received > 0, "Receive should return a positive number of bytes");
        assert_true(static_cast<size_t>(bytes_received) <= test_size, "Received bytes should not exceed sent bytes");
        
        // Verify data integrity
        bool data_intact = true;
        for (int i = 0; i < bytes_received; i++) {
            if (received_data[i] != static_cast<char>(i % 256)) {
                data_intact = false;
                break;
            }
        }
        assert_true(data_intact, "Data should be received intact");
    }
}

void ProtocolTest::testMTULimitHandling() {
    typedef ProtocolInitializer::ProtocolType ProtocolImpl;
    
    // Test sending data at MTU limit
    const size_t mtu_size = ProtocolImpl::MTU;
    std::vector<char> mtu_data(mtu_size, 'A');
    
    Ethernet::Address mac1 = nic1->address();
    Ethernet::Address mac2 = nic2->address();
    auto port1 = static_cast<ProtocolImpl::Port>(1234);
    auto port2 = static_cast<ProtocolImpl::Port>(5678);
    auto src_addr = ProtocolImpl::Address(mac1, port1);
    auto dst_addr = ProtocolImpl::Address(mac2, port2);
    
    // This should handle MTU-sized data appropriately
    int bytes_sent = proto1->send(src_addr, dst_addr, mtu_data.data(), mtu_size);
    // The result depends on implementation - it might truncate, reject, or handle differently
    // We just verify it doesn't crash
}

// === ERROR HANDLING TESTS ===

void ProtocolTest::testSendWithNullData() {
    typedef ProtocolInitializer::ProtocolType ProtocolImpl;
    
    Ethernet::Address mac1 = nic1->address();
    Ethernet::Address mac2 = nic2->address();
    auto port1 = static_cast<ProtocolImpl::Port>(1234);
    auto port2 = static_cast<ProtocolImpl::Port>(5678);
    auto src_addr = ProtocolImpl::Address(mac1, port1);
    auto dst_addr = ProtocolImpl::Address(mac2, port2);
    
    // Test send with null data
    int bytes_sent = proto1->send(src_addr, dst_addr, nullptr, 100);
    assert_true(bytes_sent <= 0, "Send with null data should return 0 or negative value");
}

void ProtocolTest::testSendWithZeroSize() {
    typedef ProtocolInitializer::ProtocolType ProtocolImpl;
    
    Ethernet::Address mac1 = nic1->address();
    Ethernet::Address mac2 = nic2->address();
    auto port1 = static_cast<ProtocolImpl::Port>(1234);
    auto port2 = static_cast<ProtocolImpl::Port>(5678);
    auto src_addr = ProtocolImpl::Address(mac1, port1);
    auto dst_addr = ProtocolImpl::Address(mac2, port2);
    
    const char* test_data = "test";
    
    // Test send with zero size
    int bytes_sent = proto1->send(src_addr, dst_addr, test_data, 0);
    assert_true(bytes_sent <= 0, "Send with zero size should return 0 or negative value");
}

void ProtocolTest::testReceiveWithNullBuffer() {
    // This test is covered in testReceiveWithInvalidBuffer
}

// === THREAD SAFETY TESTS ===

void ProtocolTest::testConcurrentSendOperations() {
    typedef ProtocolInitializer::ProtocolType ProtocolImpl;
    
    const int num_threads = 4;
    const int messages_per_thread = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successful_sends(0);
    
    Ethernet::Address mac1 = nic1->address();
    Ethernet::Address mac2 = nic2->address();
    auto port1 = static_cast<ProtocolImpl::Port>(1234);
    auto port2 = static_cast<ProtocolImpl::Port>(5678);
    auto src_addr = ProtocolImpl::Address(mac1, port1);
    auto dst_addr = ProtocolImpl::Address(mac2, port2);
    
    // Create threads that send messages concurrently
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < messages_per_thread; j++) {
                std::string message = "Thread " + std::to_string(i) + " Message " + std::to_string(j);
                int bytes_sent = proto1->send(src_addr, dst_addr, message.c_str(), message.length() + 1);
                if (bytes_sent > 0) {
                    successful_sends++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify that most sends were successful (allowing for some failures due to concurrency)
    assert_true(successful_sends.load() > (num_threads * messages_per_thread) / 2, 
                "At least half of concurrent sends should be successful");
}

void ProtocolTest::testConcurrentObserverOperations() {
    typedef ProtocolInitializer::ProtocolType ProtocolImpl;
    
    const int num_observers = 4;
    std::vector<std::thread> threads;
    std::vector<ProtocolObserver<ProtocolImpl>*> test_observers;
    
    Ethernet::Address mac2 = nic2->address();
    auto port2 = static_cast<ProtocolImpl::Port>(5678);
    auto dst_addr = ProtocolImpl::Address(mac2, port2);
    
    // Create observers
    for (int i = 0; i < num_observers; i++) {
        auto observer = new ProtocolObserver<ProtocolImpl>(port2);
        test_observers.push_back(observer);
        observers.push_back(observer); // For cleanup
    }
    
    // Create threads that attach/detach observers concurrently
    for (int i = 0; i < num_observers; i++) {
        threads.emplace_back([&, i]() {
            // Attach observer
            ProtocolImpl::attach(test_observers[i], dst_addr);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            // Detach observer
            ProtocolImpl::detach(test_observers[i], dst_addr);
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Test passes if no crashes occur during concurrent operations
}

void ProtocolTest::testConcurrentSendReceiveOperations() {
    typedef ProtocolInitializer::ProtocolType ProtocolImpl;
    
    // Create observer for receiving
    Ethernet::Address mac2 = nic2->address();
    auto port2 = static_cast<ProtocolImpl::Port>(5678);
    auto dst_addr = ProtocolImpl::Address(mac2, port2);
    
    auto observer = new ProtocolObserver<ProtocolImpl>(port2);
    observers.push_back(observer);
    ProtocolImpl::attach(observer, dst_addr);
    
    std::atomic<bool> stop_test(false);
    std::atomic<int> messages_sent(0);
    std::atomic<int> messages_received(0);
    
    // Sender thread
    std::thread sender_thread([&]() {
        Ethernet::Address mac1 = nic1->address();
        auto port1 = static_cast<ProtocolImpl::Port>(1234);
        auto src_addr = ProtocolImpl::Address(mac1, port1);
        
        int count = 0;
        while (!stop_test.load() && count < 50) {
            std::string message = "Message " + std::to_string(count);
            int bytes_sent = proto1->send(src_addr, dst_addr, message.c_str(), message.length() + 1);
            if (bytes_sent > 0) {
                messages_sent++;
            }
            count++;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    // Receiver thread
    std::thread receiver_thread([&]() {
        while (!stop_test.load() && messages_received.load() < 25) {
            if (observer->waitForData(100)) {
                messages_received++;
                observer->resetData();
            }
        }
    });
    
    // Let test run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    stop_test = true;
    
    sender_thread.join();
    receiver_thread.join();
    
    // Verify some messages were sent and received
    assert_true(messages_sent.load() > 0, "Some messages should have been sent");
    assert_true(messages_received.load() > 0, "Some messages should have been received");
}

// === IMPLEMENTATION OF HELPER CLASSES ===

ProtocolInitializer::NICType* ProtocolInitializer::create_test_nic(unsigned int id) {
    // Setting virtual MAC Address
    Ethernet::Address addr = create_test_address(id);
    
    NICType* nic = create_nic();
    nic->setAddress(addr);
    return nic;
}

ProtocolInitializer::ProtocolType* ProtocolInitializer::create_test_protocol(NICType* nic) {
    return create_protocol(nic);
}

Ethernet::Address ProtocolInitializer::create_test_address(unsigned int id) {
    Ethernet::Address addr;
    addr.bytes[0] = 0x02; // local, unicast
    addr.bytes[1] = 0x00;
    addr.bytes[2] = 0x00;
    addr.bytes[3] = 0x00;
    addr.bytes[4] = (id >> 8) & 0xFF;
    addr.bytes[5] = id & 0xFF;
    return addr;
}

int main() {
    ProtocolTest test;
    test.run();
    return 0;
} 