#include "../testcase.h"
#include "api/network/initializer.h"

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
                
        void test_send_message();
        void test_send_when_closed();
        
        void test_receive_message();
        void test_receive_when_closed();
    
    private:
        static NIC_T* _nic;
        static Protocol_T* _protocol;
        Communicator_T* _comms;
};

NIC<SocketEngine>* TestCommunicator::_nic;
Protocol<NIC<SocketEngine>>* TestCommunicator::_protocol;

TestCommunicator::TestCommunicator() {
    DEFINE_TEST(test_creation_with_null_channel);
    DEFINE_TEST(test_send_message);
    DEFINE_TEST(test_send_when_closed);
    DEFINE_TEST(test_receive_message);
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
    _comms = new Communicator_T(_protocol, Protocol_T::Address(_nic->address(), 0));
}

void TestCommunicator::tearDown() {
    delete _comms;
}
/*******************************/

/************ TESTS ************/
void TestCommunicator::test_creation_with_null_channel() {
    // Exercise SUT
    assert_throw<std::invalid_argument>([] { Communicator_T(nullptr, Protocol_T::Address(_nic->address(), 0)); });
}

void TestCommunicator::test_send_message() {
    // Inline Setup
    Message msg(Message::Type::INTEREST, _comms->address(), 0, Message::Microseconds(10));

    // Exercise SUT
    bool result = _comms->send(&msg);

    // Result Verification
    assert_true(result, "Communicator failed to send valid message!");
}

void TestCommunicator::test_send_when_closed() {
    // Exercise SUT
    _comms->release();

    // Result Verification
    Message msg(Message::Type::INTEREST, _comms->address(), 0, Message::Microseconds(10));
    assert_false(_comms->send(&msg), "Communicator was not closed!");
}

void TestCommunicator::test_receive_message() {
    // Inline Fixture
    NIC_T* sender_nic = create_nic();
    Ethernet::Address sender_addr = {{0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E}};
    sender_nic->setAddress(sender_addr);

    Protocol_T* sender_protocol = create_protocol(sender_nic);
    
    Communicator_T sender_comms(sender_protocol, Protocol<NIC<SocketEngine>>::Address(sender_nic->address(), Protocol<NIC<SocketEngine>>::Address::NULL_VALUE));

    Message send_msg(Message::Type::INTEREST, _comms->address(), 0, Message::Microseconds(10));

    sender_comms.send(&send_msg);

    Message msg;

    // Exercise SUT
    bool result = _comms->receive(&msg);

    // Result Verification
    assert_true(result, "Communicator::receive() returned false even though a valid message was sent!");
    assert_true(send_msg.origin() == msg.origin(), "Received message does not have the same origin address as the message sent!");
    assert_true(send_msg.message_type() == msg.message_type(), "Received message does not have the same message type as the message sent!");
    assert_true(send_msg.timestamp() == msg.timestamp(), "Received message does not have the same timestamp as the message sent!");
    assert_true(send_msg.period() == msg.period(), "Received message does not have the same period as the message sent!");

    // TearDown
    sender_nic->stop();

    delete sender_protocol;
    delete sender_nic;
}


void TestCommunicator::test_receive_when_closed() {
    // Inline Fixture
    _comms->release();
    Message msg;

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