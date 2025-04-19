#include <pthread.h>
#include <unistd.h>
#include <atomic>
#include <iostream>

#include "../testcase.h"
#include "test_utils.h"
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
        void test_receive_null_buffer();
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
    /* DEFINE_TEST(test_send_valid_message);
    DEFINE_TEST(test_send_empty_message);
    DEFINE_TEST(test_send_null_message);
    DEFINE_TEST(test_send_when_closed);
    DEFINE_TEST(test_receive_valid_message);
    DEFINE_TEST(test_receive_null_message);
    DEFINE_TEST(test_receive_null_buffer);
    DEFINE_TEST(test_receive_when_closed); */

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
/********************************/


int main() {

    TestCommunicator test;
    test.run();
}