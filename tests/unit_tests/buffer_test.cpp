#include <iostream>
#include <string>

#include "../test_utils.h"
#include "../testcase.h"
#include "util/buffer.h"

#define DEFINE_TEST(name) registerTest(#name, [this]() { this->name(); });

class TestBuffer : public TestCase {
    public:
        TestBuffer();
        ~TestBuffer() = default;

        void setUp() override;
        void tearDown() override;

        /* TESTS */
        void test_create_empty_buffer();
        void test_create_buffer_with_data();

        void test_set_and_retrieve_data();
        

        void test_clear_buffer();

    public:
        struct TestData {
            int value1;
            std::string value2;
            double value3;
        
            bool operator==(const TestData& other) const {
                return value1 == other.value1 && 
                    value2 == other.value2 && 
                    value3 == other.value3;
            }
        };

        typedef Buffer<TestData> BufferT; 

    private:
        BufferT* _buf;
};

std::ostream& operator<<(std::ostream& os, const TestBuffer::TestData& data) {
    os << "{ value1: " << data.value1
       << ", value2: \"" << data.value2 << "\""
       << ", value3: " << data.value3
       << " }";
    return os;
}

TestBuffer::TestBuffer() {
    DEFINE_TEST(test_create_empty_buffer);
    DEFINE_TEST(test_create_buffer_with_data);
    DEFINE_TEST(test_set_and_retrieve_data);
    DEFINE_TEST(test_clear_buffer);
}

/******* FIXTURES METHODS ******/
void TestBuffer::setUp() {
    _buf = new BufferT();
}

void TestBuffer::tearDown() {
    delete _buf;
}
/*******************************/

/************ TESTS ************/
void TestBuffer::test_create_empty_buffer() {
    // Inline Fixture
    const unsigned int empty_size = 0;

    // Exercise SUT
    BufferT buf;

    // Result Verification
    assert_equal(buf.size(), empty_size, "Empty buffer initialized with size != 0!");
}

void TestBuffer::test_create_buffer_with_data() {
    // Inline Fixture
    TestData data;
    data.value1 = 1;
    data.value2 = "teste";
    data.value3 = 3.14;

    // Exercise SUT
    BufferT buf;
    buf.setData(&data, sizeof(TestData));

    // Result Verification
    assert_equal(buf.size(), sizeof(TestData), "Buffer size is different of data size!");
}

void TestBuffer::test_set_and_retrieve_data() {
    // Inline Fixture
    TestData data;
    data.value1 = 1;
    data.value2 = "teste";
    data.value3 = 3.14;

    // Exercise SUT
    _buf->setData(&data, sizeof(TestData));

    // Result Verification
    assert_equal(*_buf->data(), data, "Retrieved buffer data is different of original data!");
    assert_equal(_buf->size(), sizeof(TestData), "Buffer size id different of data size!");
}

void TestBuffer::test_clear_buffer() {
    // Inline Fixture
    const unsigned int empty_size = 0;

    TestData data;
    data.value1 = 1;
    data.value2 = "teste";
    data.value3 = 3.14;

    _buf->setData(&data, sizeof(TestData));

    // Exercise SUT
    _buf->clear();

    // Result Verification
    assert_equal(_buf->size(), empty_size, "Buffer size is not zero after cleaned!");
}


/*******************************/
int main() {
    TestBuffer test;
    test.run();
} 