#include <iostream>
#include <string>
#include <cstdint>

#include "../test_utils.h"
#include "../testcase.h"
#include "api/framework/periodicThread.h"


class PeriodicThreadTest : public TestCase {
    public:
        PeriodicThreadTest();
        ~PeriodicThreadTest() = default;

        void setUp() override;
        void tearDown() override;

        /* TESTS */
        void test_periodic_thread_creation();
        void test_periodic_thread_execution();
        void test_periodic_thread_period_update();
        void test_periodic_thread_interruption();
        void run_in_thread();

    private:
        Periodic_Thread<PeriodicThreadTest>* _periodic_thread;
};

PeriodicThreadTest::PeriodicThreadTest() : _periodic_thread(nullptr) {
    DEFINE_TEST(test_periodic_thread_creation);
    DEFINE_TEST(test_periodic_thread_execution);
    DEFINE_TEST(test_periodic_thread_period_update);
    DEFINE_TEST(test_periodic_thread_interruption);
}

void PeriodicThreadTest::setUp() {}

void PeriodicThreadTest::tearDown() {}

void PeriodicThreadTest::run_in_thread() {
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Simulate some work in the thread
}

void PeriodicThreadTest::test_periodic_thread_creation() {
    _periodic_thread = new Periodic_Thread<PeriodicThreadTest>(this, &PeriodicThreadTest::run_in_thread);
    assert_true(!_periodic_thread->running(), "Periodic thread should not be running after creation");
}

void PeriodicThreadTest::test_periodic_thread_execution() {
    assert_true(!_periodic_thread->running(), "Periodic thread should not be running before start");
    std::int64_t initial_period = 500; // Initial period in microseconds
    _periodic_thread->start(initial_period); // Start with a period of 500 microseconds
    assert_true(_periodic_thread->running(), "Periodic thread should be running after start");
    assert_true(_periodic_thread->period() == initial_period, "Periodic thread should have the correct initial period");

    // Simulate some work
    std::this_thread::sleep_for(std::chrono::milliseconds(initial_period));
}

void PeriodicThreadTest::test_periodic_thread_period_update() {
    _periodic_thread->adjust_period(750);
    assert_true(_periodic_thread->period() == 250, "Periodic thread should have the updated period");
}

void PeriodicThreadTest::test_periodic_thread_interruption() {
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    _periodic_thread->join();
    
    assert_true(!_periodic_thread->running(), "Periodic thread should not be running after join");
    _periodic_thread = nullptr; // Prevent double deletion
}

int main() {
    PeriodicThreadTest test;
    test.run();
    
    return 0;
}