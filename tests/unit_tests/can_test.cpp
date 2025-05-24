#include <iostream>
#include <string>
#include <filesystem>

#include "test_utils.h"
#include "../testcase.h"
#include "api/framework/bus.h"
#include "../stubs/agent_stub.h"

// Helper function to set up a vehicle log directory
std::string setup_log_directory() {
    std::string log_file;
    std::error_code ec;
    
    // Try in priority order: Docker logs dir, tests/logs dir, current dir
    if (std::filesystem::exists("tests/logs")) {
        std::string test_dir = "test/logs/can_test";
        std::filesystem::create_directory(test_dir, ec);
        
        if (!ec) {
            return "test/logs/can_test.log";
        }
    }
    
    // Try tests/logs directory
    std::string test_logs_dir = "tests/logs/can_test";
    
    try {
        std::filesystem::create_directories(test_logs_dir);
        return "test/logs/can_test1/can_test.log";
    } catch (...) {
        // Fallback to tests/logs without vehicle subfolder
        return "tests/logs/can_test1.log";
    }
}

class CANTest : public TestCase {
    public:
        enum class DataTypes : std::uint32_t {
            TIPO1,
            TIPO2,
            TIPO3,
            TIPO4,
        };

        CANTest();
        ~CANTest() = default;

        void setUp() override;
        void tearDown() override;

        /* TESTS */
        void test_send_no_observer();
        void test_send_and_receive();

    private:
        CAN* _can;
        AgentStub* _agent1;
        AgentStub* _agent2;
        AgentStub* _agent3;
};

CANTest::CANTest() {
    DEFINE_TEST(test_send_no_observer);
    DEFINE_TEST(test_send_and_receive);
}

void CANTest::setUp() {
    _can = new CAN();
    _agent1 = new AgentStub(_can, static_cast<std::uint32_t>(DataTypes::TIPO1), static_cast<std::uint32_t>(DataTypes::TIPO3));
    _agent2 = new AgentStub(_can, static_cast<std::uint32_t>(DataTypes::TIPO3), static_cast<std::uint32_t>(DataTypes::TIPO1));
    _agent3 = new AgentStub(_can, static_cast<std::uint32_t>(DataTypes::TIPO3), static_cast<std::uint32_t>(DataTypes::TIPO1));
}

void CANTest::tearDown() {
    delete _agent1;
    delete _agent2;
    delete _agent3;
    delete _can;
}

void CANTest::test_send_no_observer() {
    // Inline Setup
    CAN::Message msg(CAN::Message::Type::INTEREST, CAN::Address(), static_cast<std::uint32_t>(DataTypes::TIPO4));

    // Exercise SUT
    int result = _can->send(&msg);

    // Result Verification
    assert_equal(0, result, "CAN::send sent the message, when it should not notify anyone");
}

void CANTest::test_send_and_receive() {
    // Inline Setup
    CAN::Message::Microseconds::rep period = 2;
    
    // Exercise SUT
    int result = _agent1->send(CAN::Message::Microseconds(period));
    
    // Result Verification
    assert_true(result != 0, "Message was not sent, but two agents should've been notified");

    CAN::Message msg;
    _agent1->receive(&msg);
    msg.period();
    assert_equal(period, msg.period().count(), "Received message should have the same period");
}

int main () {
    std::string log_file = setup_log_directory();
    Debug::set_log_file(log_file);
    CANTest test;
    test.run();
}