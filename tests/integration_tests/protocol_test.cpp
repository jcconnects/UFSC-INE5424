#include "initializer.h"
#include "../testcase.h"
#include <vector>
#include <sys/types.h>
#include <sys/wait.h>

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
        // void test_peek();
        // void test_attach();
        // void test_detach();

    private:
        std::vector<NIC*> _nics;
        std::vector<Protocol*> _protocols;
        static const size_t NUM_PROTOCOLS = 3; // Number of protocols to create
};

ProtocolTest::ProtocolTest() {
    // DEFINE_TEST(test_send);
    DEFINE_TEST(test_receive);
    // DEFINE_TEST(test_peek);
    // DEFINE_TEST(test_attach);
    // DEFINE_TEST(test_detach);
}

/******* FIXTURES METHODS ******/
void ProtocolTest::setUp() {
    
    // Create multiple protocol instances
    for (size_t i = 0; i < NUM_PROTOCOLS; i++) {
        _nics.push_back(Initializer::create_nic());
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
}

void ProtocolTest::test_receive() {
    std::cout << "\nENABLE the debug traits to see the results of this test\n" << std::endl;
    // Setup test data
    std::string original_message = "test serialized message";
    size_t msg_size = original_message.size();
    
    // Create sender and receiver
    auto sender_nic = _nics[0];
    auto sender_protocol = _protocols[0];
    auto receiver_nic = _nics[1];
    auto receiver_protocol = _protocols[1];

    // Setup sender address
    Protocol::Address sender_address(sender_nic->address(), 1);
    
    // Setup receiver address
    Protocol::Address receiver_address(receiver_nic->address(), 2);
    
    // Allocate buffer for sending
    NIC::DataBuffer* send_buffer = sender_nic->alloc(NIC::BROADCAST, 888, msg_size);
    
    // "Serialize" by copying bytes to payload
    std::memcpy(send_buffer->data()->payload, original_message.c_str(), msg_size);
    
    // Create receiving buffer
    NIC::DataBuffer* recv_buffer = receiver_nic->alloc(NIC::BROADCAST, 888, msg_size);
    
    // Create buffer for received data
    std::uint8_t received_data[msg_size];
    
    // Send serialized data
    sender_protocol->send(
        sender_address,
        receiver_address,
        send_buffer->data()->payload, 
        msg_size
    );
    
    // Receive the data
    receiver_protocol->receive(
        recv_buffer,
        nullptr,
        received_data,
        msg_size
    );
    
    // Verify the data
    std::string received_message(reinterpret_cast<char*>(received_data), msg_size);
    
    assert_equal(original_message, received_message, 
                "Serialized message didn't match after send/receive!");
}


int main() {
    ProtocolTest test;
    test.run();
    return 0;
}