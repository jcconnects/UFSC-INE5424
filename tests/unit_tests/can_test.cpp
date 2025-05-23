#include <iostream>
#include <string>

#include "test_utils.h"
#include "../testcase.h"
#include "bus.h"
#include "agent_stub.h"
#include "message.h"

class TestCAN : public TestCase {
    public:
        TestCAN();
        ~TestCAN() = default;

        void setUp() override;
        void tearDown() override;

        /* TESTS */
        void test_send_no_observer();
        void test_send_and_receive()

    private:
        AgentStub* _agent1;
        AgentStub* _agent2;
        AgentStub* _agent3;
};

TestCAN::TestCAN() {
    DEFINE_TEST(test_send_no_observer);
    DEFINE_TEST(test_send_and_receive);
}

void TestCAN::setUp() {
    CAN* can = CAN::get_can();
    _agent1 = new AgentStub(can);
    _agent2 = new AgentStub(can);
    _agent3 = new AgentStub(can);
}

void TestCAN::tearDown() {
    delete _agent1;
    delete _agent2;
    delete _agent3;
}

void TestCAN::test_send_no_observer() {
    
    assert_equal(0, result, "No observers should be notified");
}

void TestCAN::test_send_and_receive() {
    unsigned int result = _agent3->send("Any Message");
    assert_equal(2, result, "Two observers should be notified");

    Message* received_msg = _agent1->receive();
    assert_equal(msg.message_type(), received_msg->message_type(), "Received message type should match");
}

int main () {

}