#include "initializer.h"
#include "../testcase.h"
#include <vector>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <chrono>

class ProtocolTest : public TestCase {
    public:
        typedef Initializer::NIC_T NIC;
        typedef Initializer::PROTOCOL_T Protocol;

        ProtocolTest();
        ~ProtocolTest() = default;

        void setUp() override;
        void tearDown() override;

        void test_send();
        void test_receive();
        void test_internal_broadcast();
        void test_message_types();
        void test_port_filtering();

    private:
        std::vector<NIC*> _nics;
        std::vector<Protocol*> _protocols;
        static const size_t NUM_PROTOCOLS = 3; // Number of protocols to create
};

ProtocolTest::ProtocolTest() {
    DEFINE_TEST(test_send);
    DEFINE_TEST(test_receive);
    DEFINE_TEST(test_internal_broadcast);
    DEFINE_TEST(test_message_types);
    DEFINE_TEST(test_port_filtering);
}

/******* FIXTURES METHODS ******/
void ProtocolTest::setUp() {
    
    // Create multiple protocol instances
    for (size_t i = 0; i < NUM_PROTOCOLS; i++) {
        _nics.push_back(Initializer::create_nic());
        
        // Set unique MAC addresses for each NIC
        Ethernet::Address addr = {{0x00, 0x1A, 0x2B, 0x3C, 0x4D, static_cast<std::uint8_t>(0x5E + i)}};
        _nics[i]->setAddress(addr);
        
        _protocols.push_back(Initializer::create_protocol(_nics[i]));
    }
}

void ProtocolTest::tearDown() {
    // Clean up all protocol instances
    for (auto protocol : _protocols) {
        delete protocol;
    }
    _protocols.clear();
    
    for (auto nic : _nics) {
        nic->stop();
    }
    _nics.clear();
}
/*******************************/

/************ TESTS ************/
void ProtocolTest::test_send() {
    // Test basic sending functionality
    std::cout << "Testing basic send..." << std::endl;
    
    // Setup test data
    std::string original_message = "test serialized message";
    size_t msg_size = original_message.size();
    
    // Use protocol 0 as sender
    auto sender_nic = _nics[0];
    auto sender_protocol = _protocols[0];
    
    // Create sender and receiver addresses
    Protocol::Address sender_address(sender_nic->address(), 5);  // Port 5 for sender
    Protocol::Address receiver_address(Ethernet::BROADCAST, 0);  // Gateway port (0)
    
    // Allocate buffer for sending
    NIC::DataBuffer* send_buffer = sender_nic->alloc(receiver_address.paddr(), 888, msg_size);
    
    // Copy data to payload
    std::memcpy(send_buffer->data()->payload, original_message.c_str(), msg_size);
    
    // Send the data
    bool sent = sender_protocol->send(
        sender_address,
        receiver_address,
        send_buffer->data()->payload, 
        msg_size
    );
    
    assert_true(sent, "Failed to send message");
    
    // Clean up
    sender_nic->free(send_buffer);
}

void ProtocolTest::test_receive() {
    std::cout << "\nTesting basic receive functionality\n" << std::endl;
    
    // Setup test data
    std::string original_message = "test serialized message";
    size_t msg_size = original_message.size();
    
    // Create sender and receiver
    auto sender_nic = _nics[0];
    auto sender_protocol = _protocols[0];
    auto receiver_nic = _nics[1];
    auto receiver_protocol = _protocols[1];

    // Print addresses for debugging
    std::cout << "Sender MAC: " << sender_nic->address().to_string() << std::endl;
    std::cout << "Receiver MAC: " << receiver_nic->address().to_string() << std::endl;

    // Setup sender address with port 5
    Protocol::Address sender_address(sender_nic->address(), 5);
    
    // Setup receiver address with port 6
    Protocol::Address receiver_address(receiver_nic->address(), 6);
    
    // Allocate buffer for sending
    NIC::DataBuffer* send_buffer = sender_nic->alloc(receiver_address.paddr(), 888, msg_size);
    
    // "Serialize" by copying bytes to payload
    std::memcpy(send_buffer->data()->payload, original_message.c_str(), msg_size);
    
    // Create receiving buffer
    NIC::DataBuffer* recv_buffer = receiver_nic->alloc(Ethernet::BROADCAST, 888, msg_size);
    
    // Create buffer for received data
    std::vector<std::uint8_t> received_data(msg_size);
    
    // Create a thread to receive the data (non-blocking for test)
    std::thread receive_thread([&]() {
        // Wait a bit for the sender to be ready
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Receive the data (blocks until data arrives)
        bool received = receiver_protocol->receive(
            recv_buffer,
            nullptr,
            received_data.data(),
            msg_size
        );
        
        assert_true(received, "Failed to receive message");
    });
    
    // Send data from sender to receiver
    bool sent = sender_protocol->send(
        sender_address,
        receiver_address,
        send_buffer->data()->payload, 
        msg_size
    );
    
    assert_true(sent, "Failed to send message");
    
    // Wait for receive to complete
    receive_thread.join();
    
    // Verify the data
    std::string received_message(reinterpret_cast<char*>(received_data.data()), msg_size);
    
    assert_equal(original_message, received_message, 
                "Serialized message didn't match after send/receive!");
                
    // Clean up
    sender_nic->free(send_buffer);
    receiver_nic->free(recv_buffer);
}

void ProtocolTest::test_internal_broadcast() {
    std::cout << "\nTesting internal broadcast functionality\n" << std::endl;
    
    // Setup test data
    std::string original_message = "broadcast message";
    size_t msg_size = original_message.size();
    
    // Use protocol 0 as sender (producer)
    auto sender_nic = _nics[0];
    auto sender_protocol = _protocols[0];
    
    // Use protocol 1 and 2 as receivers (consumers)
    auto receiver1_nic = _nics[1];
    auto receiver1_protocol = _protocols[1];
    auto receiver2_nic = _nics[2];
    auto receiver2_protocol = _protocols[2];
    
    // Create sender and broadcast addresses
    Protocol::Address sender_address(sender_nic->address(), 5);     // Port 5 (producer)
    Protocol::Address broadcast_address(Ethernet::BROADCAST, 0);    // Port 0 (gateway)
    
    // Create receiver addresses - both on port 1 (consumers)
    Protocol::Address receiver1_address(receiver1_nic->address(), 1);
    Protocol::Address receiver2_address(receiver2_nic->address(), 1);
    
    // Allocate buffer for sending to gateway
    NIC::DataBuffer* send_buffer = sender_nic->alloc(broadcast_address.paddr(), 888, msg_size);
    
    // Copy data to payload
    std::memcpy(send_buffer->data()->payload, original_message.c_str(), msg_size);
    
    // Create receiving buffers for each receiver
    NIC::DataBuffer* recv1_buffer = receiver1_nic->alloc(Ethernet::BROADCAST, 888, msg_size);
    NIC::DataBuffer* recv2_buffer = receiver2_nic->alloc(Ethernet::BROADCAST, 888, msg_size);
    
    // Prepare data containers for receivers
    std::vector<std::uint8_t> recv1_data(msg_size);
    std::vector<std::uint8_t> recv2_data(msg_size);
    
    // Create threads for receivers
    std::thread receiver1_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        bool received = receiver1_protocol->receive(
            recv1_buffer,
            nullptr,
            recv1_data.data(),
            msg_size
        );
        assert_true(received, "Receiver 1 failed to receive broadcast");
    });
    
    std::thread receiver2_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        bool received = receiver2_protocol->receive(
            recv2_buffer,
            nullptr,
            recv2_data.data(),
            msg_size
        );
        assert_true(received, "Receiver 2 failed to receive broadcast");
    });
    
    // Give receivers time to start listening
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Sender broadcasts to gateway port
    bool sent = sender_protocol->send(
        sender_address,
        broadcast_address,  // Gateway address for internal broadcast
        send_buffer->data()->payload, 
        msg_size
    );
    
    assert_true(sent, "Failed to send broadcast message");
    
    // Wait for receivers to complete
    receiver1_thread.join();
    receiver2_thread.join();
    
    // Verify the received data matches for both receivers
    std::string recv1_message(reinterpret_cast<char*>(recv1_data.data()), msg_size);
    std::string recv2_message(reinterpret_cast<char*>(recv2_data.data()), msg_size);
    
    assert_equal(original_message, recv1_message, "Receiver 1 received incorrect message");
    assert_equal(original_message, recv2_message, "Receiver 2 received incorrect message");
    
    // Clean up
    sender_nic->free(send_buffer);
    receiver1_nic->free(recv1_buffer);
    receiver2_nic->free(recv2_buffer);
}

void ProtocolTest::test_message_types() {
    std::cout << "\nTesting message type handling\n" << std::endl;
    
    // Test that we can properly send/receive both INTEREST and RESPONSE message types
    
    // Create basic interest message data
    std::string interest_prefix = "INTEREST:";
    std::string interest_message = interest_prefix + "VEHICLE_SPEED";
    size_t interest_size = interest_message.size();
    
    // Create basic response message data
    std::string response_prefix = "RESPONSE:";
    std::string response_value = "123.45";
    std::string response_message = response_prefix + response_value;
    size_t response_size = response_message.size();
    
    // Use protocol 0 as consumer sending INTEREST
    auto consumer_nic = _nics[0];
    auto consumer_protocol = _protocols[0];
    
    // Use protocol 1 as producer responding with RESPONSE
    auto producer_nic = _nics[1];
    auto producer_protocol = _protocols[1];
    
    // Create addresses
    Protocol::Address consumer_address(consumer_nic->address(), 1);  // Port 1 for consumer
    Protocol::Address producer_address(producer_nic->address(), 2);  // Port 2 for producer
    
    // --- Step 1: Consumer sends INTEREST to Producer ---
    
    // Allocate buffer for interest
    NIC::DataBuffer* interest_buffer = consumer_nic->alloc(producer_address.paddr(), 888, interest_size);
    
    // Copy interest data to payload
    std::memcpy(interest_buffer->data()->payload, interest_message.c_str(), interest_size);
    
    // Create buffer for producer to receive interest
    NIC::DataBuffer* recv_interest_buffer = producer_nic->alloc(Ethernet::BROADCAST, 888, interest_size);
    std::vector<std::uint8_t> recv_interest_data(interest_size);
    
    // Create thread for producer to receive interest
    std::thread producer_receive_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        bool received = producer_protocol->receive(
            recv_interest_buffer,
            nullptr,
            recv_interest_data.data(),
            interest_size
        );
        assert_true(received, "Producer failed to receive interest message");
    });
    
    // Consumer sends interest to producer
    bool interest_sent = consumer_protocol->send(
        consumer_address,
        producer_address,
        interest_buffer->data()->payload, 
        interest_size
    );
    
    assert_true(interest_sent, "Failed to send interest message");
    
    // Wait for producer to receive interest
    producer_receive_thread.join();
    
    // Verify interest was received correctly
    std::string received_interest(reinterpret_cast<char*>(recv_interest_data.data()), interest_size);
    assert_equal(interest_message, received_interest, "Interest message corrupted in transit");
    
    // --- Step 2: Producer sends RESPONSE back to Consumer ---
    
    // Allocate buffer for response
    NIC::DataBuffer* response_buffer = producer_nic->alloc(consumer_address.paddr(), 888, response_size);
    
    // Copy response data to payload
    std::memcpy(response_buffer->data()->payload, response_message.c_str(), response_size);
    
    // Create buffer for consumer to receive response
    NIC::DataBuffer* recv_response_buffer = consumer_nic->alloc(Ethernet::BROADCAST, 888, response_size);
    std::vector<std::uint8_t> recv_response_data(response_size);
    
    // Create thread for consumer to receive response
    std::thread consumer_receive_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        bool received = consumer_protocol->receive(
            recv_response_buffer,
            nullptr,
            recv_response_data.data(),
            response_size
        );
        assert_true(received, "Consumer failed to receive response message");
    });
    
    // Producer sends response to consumer
    bool response_sent = producer_protocol->send(
        producer_address,
        consumer_address,
        response_buffer->data()->payload, 
        response_size
    );
    
    assert_true(response_sent, "Failed to send response message");
    
    // Wait for consumer to receive response
    consumer_receive_thread.join();
    
    // Verify response was received correctly
    std::string received_response(reinterpret_cast<char*>(recv_response_data.data()), response_size);
    assert_equal(response_message, received_response, "Response message corrupted in transit");
    
    // Clean up
    consumer_nic->free(interest_buffer);
    producer_nic->free(recv_interest_buffer);
    producer_nic->free(response_buffer);
    consumer_nic->free(recv_response_buffer);
}

void ProtocolTest::test_port_filtering() {
    std::cout << "\nTesting port filtering\n" << std::endl;
    
    // This test verifies that messages are properly filtered by port number
    
    // Setup test data for different ports
    std::string port1_message = "message for port 1";
    std::string port2_message = "message for port 2";
    size_t port1_size = port1_message.size();
    size_t port2_size = port2_message.size();
    
    // Use protocol 0 as sender
    auto sender_nic = _nics[0];
    auto sender_protocol = _protocols[0];
    
    // Use protocol 1 as receiver
    auto receiver_nic = _nics[1];
    auto receiver_protocol = _protocols[1];
    
    // Create addresses for different ports on same physical address
    Protocol::Address sender_address(sender_nic->address(), 3);  // Port 3 for sender
    
    // Receiver has multiple ports
    Protocol::Address receiver_port1(receiver_nic->address(), 1);  // Port 1
    Protocol::Address receiver_port2(receiver_nic->address(), 2);  // Port 2
    
    // --- Send message to port 1 ---
    
    // Allocate buffer for port 1 message
    NIC::DataBuffer* port1_buffer = sender_nic->alloc(receiver_port1.paddr(), 888, port1_size);
    std::memcpy(port1_buffer->data()->payload, port1_message.c_str(), port1_size);
    
    // --- Send message to port 2 ---
    
    // Allocate buffer for port 2 message
    NIC::DataBuffer* port2_buffer = sender_nic->alloc(receiver_port2.paddr(), 888, port2_size);
    std::memcpy(port2_buffer->data()->payload, port2_message.c_str(), port2_size);
    
    // --- Prepare receiver for port 1 ---
    
    // Create receiving buffer for port 1
    NIC::DataBuffer* recv_port1_buffer = receiver_nic->alloc(Ethernet::BROADCAST, 888, port1_size);
    std::vector<std::uint8_t> recv_port1_data(port1_size);
    
    // Create receiving buffer for port 2
    NIC::DataBuffer* recv_port2_buffer = receiver_nic->alloc(Ethernet::BROADCAST, 888, port2_size);
    std::vector<std::uint8_t> recv_port2_data(port2_size);
    
    // Create threads for each port receiver
    std::thread port1_thread([&]() {
        // Set protocol to listen on port 1
        Protocol::Address recv_addr(receiver_nic->address(), 1);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        bool received = receiver_protocol->receive(
            recv_port1_buffer,
            &recv_addr,  // Pass address to filter by port
            recv_port1_data.data(),
            port1_size
        );
        assert_true(received, "Port 1 failed to receive its message");
    });
    
    std::thread port2_thread([&]() {
        // Set protocol to listen on port 2
        Protocol::Address recv_addr(receiver_nic->address(), 2);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        bool received = receiver_protocol->receive(
            recv_port2_buffer,
            &recv_addr,  // Pass address to filter by port
            recv_port2_data.data(),
            port2_size
        );
        assert_true(received, "Port 2 failed to receive its message");
    });
    
    // Give receivers time to start listening
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Send messages to specific ports
    bool sent_to_port1 = sender_protocol->send(
        sender_address,
        receiver_port1,
        port1_buffer->data()->payload, 
        port1_size
    );
    
    bool sent_to_port2 = sender_protocol->send(
        sender_address,
        receiver_port2,
        port2_buffer->data()->payload, 
        port2_size
    );
    
    assert_true(sent_to_port1, "Failed to send message to port 1");
    assert_true(sent_to_port2, "Failed to send message to port 2");
    
    // Wait for receivers to complete
    port1_thread.join();
    port2_thread.join();
    
    // Verify each port received the correct message
    std::string recv_port1_message(reinterpret_cast<char*>(recv_port1_data.data()), port1_size);
    std::string recv_port2_message(reinterpret_cast<char*>(recv_port2_data.data()), port2_size);
    
    assert_equal(port1_message, recv_port1_message, "Port 1 received incorrect message");
    assert_equal(port2_message, recv_port2_message, "Port 2 received incorrect message");
    
    // Clean up
    sender_nic->free(port1_buffer);
    sender_nic->free(port2_buffer);
    receiver_nic->free(recv_port1_buffer);
    receiver_nic->free(recv_port2_buffer);
}

int main() {
    ProtocolTest test;
    test.run();
    return 0;
}