#include <pthread.h>
#include <unistd.h>
#include <atomic>
#include <iostream>
#include <chrono> // For sleep_for
#include <cstring> // For strlen

#include "../testcase.h"
#include "../test_utils.h"
#include "message.h"
#include "communicator.h"
#include "protocol.h"
#include "nic.h"
#include "socketEngine.h"
#include "ethernet.h"
#include "initializer.h"

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
        static NIC<SocketEngine>* _nic;
        static Protocol<NIC<SocketEngine>>* _protocol;
        Communicator<Protocol<NIC<SocketEngine>>>* _comms;
};

NIC<SocketEngine>* TestCommunicator::_nic;
Protocol<NIC<SocketEngine>>* TestCommunicator::_protocol;

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
    _comms = new Communicator<Protocol<NIC<SocketEngine>>>(_protocol, Protocol<NIC<SocketEngine>>::Address(_nic->address(), Protocol<NIC<SocketEngine>>::Address::NULL_VALUE));
}

void TestCommunicator::tearDown() {
    delete _comms;
}
/*******************************/

/************ TESTS ************/
void TestCommunicator::test_creation_with_null_channel() {
    // Exercise SUT
    assert_throw<std::invalid_argument>([] { Communicator<Protocol<NIC<SocketEngine>>>(nullptr, Protocol<NIC<SocketEngine>>::Address(_nic->address(), Protocol<NIC<SocketEngine>>::Address::NULL_VALUE)); });
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
    Message<Protocol<NIC<SocketEngine>>::MTU> msg(data.c_str(), data.size());

    // Exercise SUT
    bool result = _comms->send(&msg);

    // Result Verification
    assert_true(result, "Communicator failed to send valid message!");
}

void TestCommunicator::test_send_empty_message() {
    // Inline Fixture
    Message<Protocol<NIC<SocketEngine>>::MTU> msg;

    // Exercise SUT
    bool result = _comms->send(&msg);

    // Result Verification
    assert_false(result, "Communicator sent empty message, which should not happen!");
}

void TestCommunicator::test_send_null_message() {
    // Exercise SUT
    bool result = _comms->send(nullptr);

    // Result Verification
    assert_false(result, "Communicator sent null message, which should not happen!");
}

void TestCommunicator::test_send_when_closed() {
    // Inline Fixture
    _comms->close();
    std::string data = "teste";
    Message<Protocol<NIC<SocketEngine>>::MTU> msg(data.c_str(), data.size());

    // Exercise SUT
    bool result = _comms->send(&msg);

    // Result Verification
    assert_false(result, "Communicator sent message when closed, which should not happen!");
}

void TestCommunicator::test_receive_valid_message() {
    // Inline Fixture
    NIC<SocketEngine>* sender_nic = create_nic();
    Ethernet::Address sender_addr = {{0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E}};
    sender_nic->setAddress(sender_addr);

    Protocol<NIC<SocketEngine>>* sender_protocol = create_protocol(sender_nic);
    
    Communicator<Protocol<NIC<SocketEngine>>> sender_comms(sender_protocol, Protocol<NIC<SocketEngine>>::Address(sender_nic->address(), Protocol<NIC<SocketEngine>>::Address::NULL_VALUE));

    std::string data = "teste";
    Message<Protocol<NIC<SocketEngine>>::MTU> send_msg(data.c_str(), data.size());

    sender_comms.send(&send_msg);

    Message<Protocol<NIC<SocketEngine>>::MTU> msg;

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
    Message<Protocol<NIC<SocketEngine>>::MTU> msg;

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