#include <cstring>
#include <string>
#include "initializer.h"
#include "../testcase.h"

class NICTest : public TestCase {
    public:
        typedef Initializer::NIC_T NIC;

        NICTest();
        ~NICTest() = default;

        void setUp() override;
        void tearDown() override;

        /* TESTS */
        void test_set_address();
        void test_stop();

        void test_allocate_buffer();
        void test_allocate_empty_buffer();
        void test_allocate_way_too_big_buffer();
        void test_allocate_buffer_when_stopped();
        void test_release_buffer();
        void test_release_buffer_with_all_free();

        void test_send_internal();
        void test_send_external();
        void test_send_when_stopped();
        void test_send_null_buffer();

        void test_receive();
        void test_receive_when_stopped();
        void test_receive_null_buffer();
    
    private:
        NIC* _nic;
};

NICTest::NICTest() {
    DEFINE_TEST(test_set_address);
    DEFINE_TEST(test_stop);
    DEFINE_TEST(test_allocate_buffer);
    DEFINE_TEST(test_allocate_empty_buffer);
    DEFINE_TEST(test_allocate_way_too_big_buffer);
    DEFINE_TEST(test_allocate_buffer_when_stopped);
    DEFINE_TEST(test_release_buffer);
    DEFINE_TEST(test_send_internal);
    DEFINE_TEST(test_send_external);
    DEFINE_TEST(test_send_when_stopped);
    DEFINE_TEST(test_send_null_buffer);
    DEFINE_TEST(test_receive);
    DEFINE_TEST(test_receive_when_stopped);
    DEFINE_TEST(test_receive_null_buffer);
}

/******* FIXTURES METHODS ******/
void NICTest::setUp() {
    _nic = Initializer::create_nic();
}

void NICTest::tearDown() {
    _nic->stop();
    delete _nic;
}
/*******************************/

/************ TESTS ************/
void NICTest::test_set_address() {
    // Exercise SUT
    _nic->setAddress(NIC::BROADCAST);

    // Result Verification
    assert_equal(NIC::mac_to_string(NIC::BROADCAST), NIC::mac_to_string(_nic->address()), "NIC address was not setted");
}

void NICTest::test_stop() {
    // Exercise SUT
    _nic->stop();

    // Result Verification
    assert_false(_nic->running(), "NIC is still running after stop is called");
}

void NICTest::test_allocate_buffer() {
    // Exercise SUT
    NIC::DataBuffer* buf = _nic->alloc(NIC::BROADCAST, 888, 10);

    // Result Verification
    assert_true(buf != nullptr, "NIC did not allocate buffer for valid parameters");
}

void NICTest::test_allocate_empty_buffer() {
    // Exercise SUT
    NIC::DataBuffer* buf = _nic->alloc(NIC::BROADCAST, 888, 0);

    // Result Verification
    assert_true(buf == nullptr, "NIC allocated buffer with size equal to 0");
}

void NICTest::test_allocate_way_too_big_buffer() {
    // Exercise SUT
    NIC::DataBuffer* buf = _nic->alloc(NIC::BROADCAST, 888, NIC::MTU+1);
    
    // Result Verification
    assert_true(buf == nullptr, "NIC allocated buffer with size bigger than MTU");
}

void NICTest::test_allocate_buffer_when_stopped() {
    // Inline Setup
    _nic->stop();
    
    // Exercise SUT
    NIC::DataBuffer* buf = _nic->alloc(NIC::BROADCAST, 888, 10);
    
    // Result Verification
    assert_true(buf == nullptr, "NIC allocated buffer while stopped");
}

void NICTest::test_release_buffer() {
    // Inline Setup
    NIC::DataBuffer* buf = _nic->alloc(NIC::BROADCAST, 888, 10);

    // Exercise SUT
    _nic->free(buf);

    // Result Verification
    assert_true(buf->size() == 0, "NIC did not cleared the buffer");
    assert_equal(NIC::N_BUFFERS, _nic->buffer_pool_size(), "Buffer was cleared, but was not added to the free buffers queue");
}

void NICTest::test_send_internal() {
    // Inline Setup
    NIC::DataBuffer* buf = _nic->alloc(_nic->address(), 888, 10);

    // Exercise SUT
    int result = _nic->send(buf);

    // Result Verification
    assert_equal(buf->size(), static_cast<unsigned int>(result), "NIC failed to send valid buffer with internal engine");
}

void NICTest::test_send_external() {
    // Inline Setup
    NIC::DataBuffer* buf = _nic->alloc(NIC::BROADCAST, 888, 10);

    // Exercise SUT
    int result = _nic->send(buf);

    // Result Verification
    assert_equal(10+NIC::HEADER_SIZE, static_cast<unsigned int>(result), "NIC failed to send valid buffer with external engine");
}

void NICTest::test_send_when_stopped() {
    // Inline Setup
    NIC::DataBuffer* buf = _nic->alloc(NIC::BROADCAST, 888, 10);
    _nic->stop();

    // Exercise SUT
    int result = _nic->send(buf);

    // Result Verification
    assert_equal(-1, result, "NIC sent buffer, even though it was stopped");
}

void NICTest::test_send_null_buffer() {
    // Exercise SUT
    int result = _nic->send(nullptr);

    // Result Verification
    assert_equal(-1, result, "NIC sent null buffer");
}

void NICTest::test_receive() {
    // Inline Setup
    std::string msg = "test message";
    NIC::DataBuffer* buf = _nic->alloc(NIC::BROADCAST, 888, msg.size());
    std::memcpy(buf->data()->payload, msg.c_str(), msg.size());
    std::uint8_t temp_buffer[msg.size()];

    // Exercise SUT
    int result = _nic->receive(buf, nullptr, nullptr, temp_buffer, msg.size());

    // Result Verification
    char* received_str = reinterpret_cast<char*>(temp_buffer);
    std::string received_msg(received_str, result);

    assert_equal(msg, received_msg, "NIC failed to extract message from frame");
}

void NICTest::test_receive_when_stopped() {
    // Inline Setup
    std::string msg = "test message";
    NIC::DataBuffer* buf = _nic->alloc(NIC::BROADCAST, 888, msg.size());
    std::memcpy(buf->data()->payload, msg.c_str(), msg.size());
    std::uint8_t temp_buffer[msg.size()];
    _nic->stop();

    // Exercise SUT
    int result = _nic->receive(buf, nullptr, nullptr, temp_buffer, msg.size());

    // Result Verification
    assert_equal(msg.size(), static_cast<unsigned int>(result), "NIC failed to extract buffer content while stopped");
}

void NICTest::test_receive_null_buffer() {
    // Inline Setup
    std::string msg = "test message";
    std::uint8_t temp_buffer[msg.size()];
    
    // Exercise SUT
    int result = _nic->receive(nullptr, nullptr, nullptr, temp_buffer, msg.size());

    // Result Verification
    assert_equal(-1, result, "NIC extracted null buffer");
}
/*******************************/


int main() {
    NICTest test;
    test.run();
    return 0;
}