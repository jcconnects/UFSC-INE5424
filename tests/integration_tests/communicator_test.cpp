#include <pthread.h>
#include <unistd.h>
#include <atomic>
#include <chrono> // For sleep_for
#include <cstring> // For strlen

#include "../testcase.h"
#include "test_utils.h"
#include "initializer.h"
#include "communicator.h"

#define DEFINE_TEST(name) registerTest(#name, [this]() { this->name(); });

class TestCommunicator : public TestCase, public Initializer {

    public:
        TestCommunicator();
        ~TestCommunicator();

        static void setUpClass();
        static void tearDownClass();

        void setUp() override;
        void tearDown() override;

        /* TESTS */
        void test_creation_with_null_channel();
        
        void test_close();
        
        void test_send_valid_message();
        void test_send_empty_message();
        void test_send_null_message();
        void test_send_when_closed();
        
        void test_receive_valid_message();
        void test_receive_null_message();
        void test_receive_when_closed();
    
    private:
        static NIC<SocketEngine, SharedMemoryEngine>* _nic;
        static Protocol<NIC<SocketEngine, SharedMemoryEngine>>* _protocol;
        Communicator<Protocol<NIC<SocketEngine, SharedMemoryEngine>>>* _comms;
};

NIC<SocketEngine, SharedMemoryEngine>* TestCommunicator::_nic;
Protocol<NIC<SocketEngine, SharedMemoryEngine>>* TestCommunicator::_protocol;

TestCommunicator::TestCommunicator() {
    DEFINE_TEST(test_creation_with_null_channel);
    DEFINE_TEST(test_close);
    DEFINE_TEST(test_send_valid_message);
    DEFINE_TEST(test_send_empty_message);
    DEFINE_TEST(test_send_null_message);
    DEFINE_TEST(test_send_when_closed);
    DEFINE_TEST(test_receive_valid_message);
    DEFINE_TEST(test_receive_null_message);
    DEFINE_TEST(test_receive_when_closed);

    TestCommunicator::setUpClass();
}

TestCommunicator::~TestCommunicator() {
    TestCommunicator::tearDownClass();
}

/******** CLASS METHODS ********/
void TestCommunicator::setUpClass() {
    _nic = create_nic();
    _protocol = create_protocol(_nic);

}

void TestCommunicator::tearDownClass() {
    _nic->stop();

    delete _protocol;
    delete _nic;
}
/*******************************/

/****** FIXTURES METHODS *******/
void TestCommunicator::setUp() {
    _comms = new Communicator<Protocol<NIC<SocketEngine, SharedMemoryEngine>>>(_protocol, Protocol<NIC<SocketEngine, SharedMemoryEngine>>::Address(_nic->address(), Protocol<NIC<SocketEngine, SharedMemoryEngine>>::Address::NULL_VALUE));
}

void TestCommunicator::tearDown() {
    delete _comms;
}
/*******************************/

/************ TESTS ************/
void TestCommunicator::test_creation_with_null_channel() {
    // Exercise SUT
    assert_throw<std::invalid_argument>([] { Communicator<Protocol<NIC<SocketEngine, SharedMemoryEngine>>>(nullptr, Protocol<NIC<SocketEngine, SharedMemoryEngine>>::Address(_nic->address(), Protocol<NIC<SocketEngine, SharedMemoryEngine>>::Address::NULL_VALUE)); });
}

void TestCommunicator::test_close() {
    // Exercise SUT
    _comms->close();

    // Result Verification
    assert_true(_comms->is_closed(), "Communicator was not closed!");
}

void TestCommunicator::test_send_valid_message() {
    // Inline Fixture
    std::string data = "teste";
    Message msg(Message::Type::RESPONSE, Protocol<NIC<SocketEngine, SharedMemoryEngine>>::Address(_nic->address(), Protocol<NIC<SocketEngine, SharedMemoryEngine>>::Address::NULL_VALUE), DataTypeId::UNKNOWN, 0, data.c_str(), data.size());

    // Exercise SUT
    bool result = _comms->send(msg);

    // Result Verification
    assert_true(result, "Communicator failed to send valid message!");
}

void TestCommunicator::test_send_empty_message() {
    // Inline Fixture
    Message msg(Message::Type::RESPONSE, Protocol<NIC<SocketEngine, SharedMemoryEngine>>::Address(_nic->address(), Protocol<NIC<SocketEngine, SharedMemoryEngine>>::Address::NULL_VALUE), DataTypeId::UNKNOWN, 0, nullptr, 0);

    // Exercise SUT
    bool result = _comms->send(msg);

    // Result Verification
    assert_false(result, "Communicator sent empty message, which should not happen!");
}

void TestCommunicator::test_send_null_message() {
    // We cannot pass nullptr to a reference parameter, so we'll test this
    // condition differently by checking if the method implementation handles
    // messages with empty data properly.
    
    // Create a message with null pointer for data
    Message msg(Message::Type::RESPONSE, Protocol<NIC<SocketEngine, SharedMemoryEngine>>::Address(_nic->address(), Protocol<NIC<SocketEngine, SharedMemoryEngine>>::Address::NULL_VALUE), DataTypeId::UNKNOWN, 0, nullptr, 0);
    
    // Force serialized data to be empty
    // Note: In a real scenario, we'd need to mock Message or modify the implementation
    // Since we cannot directly manipulate private members, we'll rely on send's check for size()

    // Exercise SUT - this will call msg.size() which should return 0 if properly implemented
    bool result = _comms->send(msg);

    // Result Verification
    assert_false(result, "Communicator sent null or empty message, which should not happen!");
}

void TestCommunicator::test_send_when_closed() {
    // Inline Fixture
    _comms->close();
    std::string data = "teste";
    Message msg(Message::Type::RESPONSE, Protocol<NIC<SocketEngine, SharedMemoryEngine>>::Address(_nic->address(), Protocol<NIC<SocketEngine, SharedMemoryEngine>>::Address::NULL_VALUE), DataTypeId::UNKNOWN, 0, data.c_str(), data.size());

    // Exercise SUT
    bool result = _comms->send(msg);

    // Result Verification
    assert_false(result, "Communicator sent message when closed, which should not happen!");
}

void TestCommunicator::test_receive_valid_message() {
    // Inline Fixture
    NIC<SocketEngine, SharedMemoryEngine>* sender_nic = create_nic();
    Ethernet::Address sender_addr = {{0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E}};
    sender_nic->setAddress(sender_addr);

    Protocol<NIC<SocketEngine, SharedMemoryEngine>>* sender_protocol = create_protocol(sender_nic);
    
    Communicator<Protocol<NIC<SocketEngine, SharedMemoryEngine>>> sender_comms(sender_protocol, Protocol<NIC<SocketEngine, SharedMemoryEngine>>::Address(sender_nic->address(), Protocol<NIC<SocketEngine, SharedMemoryEngine>>::Address::NULL_VALUE));

    std::string data = "teste";
    Message send_msg(Message::Type::RESPONSE, Protocol<NIC<SocketEngine, SharedMemoryEngine>>::Address(sender_nic->address(), Protocol<NIC<SocketEngine, SharedMemoryEngine>>::Address::NULL_VALUE), DataTypeId::UNKNOWN, 0, data.c_str(), data.size());

    sender_comms.send(send_msg);

    // Use a proper constructor for the receiving message
    Message msg(Message::Type::INTEREST, Protocol<NIC<SocketEngine, SharedMemoryEngine>>::Address(_nic->address(), Protocol<NIC<SocketEngine, SharedMemoryEngine>>::Address::NULL_VALUE), DataTypeId::UNKNOWN);

    // Exercise SUT
    bool result = _comms->receive(&msg);

    // Result Verification
    assert_true(result, "Communicator::receive() returned false even though a valid message was sent!");
    std::string received(static_cast<const char*>(msg.data()), msg.size());
    assert_equal(received, "teste", "Message received is not the same message that was sent!");

    // TearDown
    sender_nic->stop();

    delete sender_protocol;
    delete sender_nic;
}

void TestCommunicator::test_receive_null_message() {
    // Exercise SUT
    bool result = _comms->receive(nullptr);

    // Result Verification
    assert_false(result, "Communicator::receive() returned true, even though a null message was passed!");
}

void TestCommunicator::test_receive_when_closed() {
    // Inline Fixture
    _comms->close();
    Message msg(Message::Type::INTEREST, Protocol<NIC<SocketEngine, SharedMemoryEngine>>::Address(_nic->address(), Protocol<NIC<SocketEngine, SharedMemoryEngine>>::Address::NULL_VALUE), DataTypeId::UNKNOWN);

    // Exercise SUT
    bool result = _comms->receive(&msg);

    // Result Verification
    assert_false(result, "Communicator received message when closed, which should not happen!");
}
/********************************/


int main() {

    TestCommunicator test;
    test.run();
}