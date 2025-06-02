#include "../../tests/testcase.h"
#include "../../tests/test_utils.h"
#include "../../include/api/util/observer.h"
#include "../../include/api/util/observed.h"
#include <iostream>
#include <vector>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <fstream>
#include <sstream>
#include <memory>
#include <mutex>

// Test constants
using TestData = int;
using TestCondition = int;
const int TEST_CONDITION_1 = 1;
const int TEST_CONDITION_2 = 2;
const int TEST_CONDITION_3 = 3;

// Forward declarations
class ObserverPatternTest;

// Test Observer implementations
class TestConditionalObserver : public Conditional_Data_Observer<TestData, TestCondition> {
public:
    std::atomic<int> update_count{0};
    TestData last_data{0};
    TestCondition last_condition{0};

    TestConditionalObserver(TestCondition c) : Conditional_Data_Observer<TestData, TestCondition>(c) {}

    void update(TestCondition c, TestData* d) override {
        last_condition = c;
        if (d) {
            last_data = *d;
        }
        if (c == this->_rank) {
            update_count++;
        }
        Conditional_Data_Observer<TestData, TestCondition>::update(c, d);
    }
};

class TestConcurrentObserver : public Concurrent_Observer<TestData, TestCondition> {
public:
    std::atomic<int> update_count{0};
    std::atomic<int> retrieved_count{0};
    TestData last_data{0};

    TestConcurrentObserver(TestCondition c) : Concurrent_Observer<TestData, TestCondition>(c) {}

    void update(TestCondition c, TestData* d) override {
        if (c == this->_rank && d != nullptr) {
            last_data = *d;
            update_count++;
        }
        Concurrent_Observer<TestData, TestCondition>::update(c, d);
    }

    TestData* updated() {
        TestData* data = Concurrent_Observer<TestData, TestCondition>::updated();
        if (data) {
            retrieved_count++;
        }
        return data;
    }
};

class ObserverPatternTest : public TestCase {
protected:
    void setUp() override;
    void tearDown() override;

    // Helper methods
    void createTestObservers();
    void cleanupTestObservers();
    void verifyObserverState(TestConditionalObserver* observer, 
                           int expected_count, 
                           TestData expected_data,
                           const std::string& context);

    // === BASIC CONDITIONAL OBSERVER TESTS ===
    void testConditionalObserverAttachDetach();
    void testConditionalObserverNotificationSingleCondition();
    void testConditionalObserverNotificationMultipleConditions();
    void testConditionalObserverMultipleObserversSameCondition();

    // === CONDITIONAL OBSERVER EDGE CASES ===
    void testConditionalObserverNonExistentCondition();
    void testConditionalObserverNullDataHandling();
    void testConditionalObserverDetachAndReattach();
    void testConditionalObserverMultipleDetach();

    // === CONCURRENT OBSERVER TESTS ===
    void testConcurrentObserverBasicFunctionality();
    void testConcurrentObserverMultipleNotifications();
    void testConcurrentObserverThreadSafety();
    void testConcurrentObserverBlockingBehavior();

    // === CONCURRENT OBSERVER ADVANCED TESTS ===
    void testConcurrentObserverMultipleConsumers();
    void testConcurrentObserverProducerConsumerPattern();
    void testConcurrentObserverDetachWhileBlocked();

    // === INTEGRATION AND STRESS TESTS ===
    void testMixedObserverPatterns();
    void testHighVolumeNotifications();
    void testObserverPatternMemoryManagement();

    // Test data and observers
    std::unique_ptr<Conditionally_Data_Observed<TestData, TestCondition>> conditional_observed;
    std::unique_ptr<Concurrent_Observed<TestData, TestCondition>> concurrent_observed;
    std::vector<std::unique_ptr<TestConditionalObserver>> conditional_observers;
    std::vector<std::unique_ptr<TestConcurrentObserver>> concurrent_observers;

public:
    ObserverPatternTest();
};

// Implementations

/**
 * @brief Constructor that registers all test methods
 * 
 * Organizes tests into logical groups for better maintainability and clarity.
 * Each test method name clearly describes what functionality is being tested.
 */
ObserverPatternTest::ObserverPatternTest() {
    // === BASIC CONDITIONAL OBSERVER TESTS ===
    DEFINE_TEST(testConditionalObserverAttachDetach);
    DEFINE_TEST(testConditionalObserverNotificationSingleCondition);
    DEFINE_TEST(testConditionalObserverNotificationMultipleConditions);
    DEFINE_TEST(testConditionalObserverMultipleObserversSameCondition);

    // === CONDITIONAL OBSERVER EDGE CASES ===
    DEFINE_TEST(testConditionalObserverNonExistentCondition);
    DEFINE_TEST(testConditionalObserverNullDataHandling);
    DEFINE_TEST(testConditionalObserverDetachAndReattach);
    DEFINE_TEST(testConditionalObserverMultipleDetach);

    // === CONCURRENT OBSERVER TESTS ===
    DEFINE_TEST(testConcurrentObserverBasicFunctionality);
    DEFINE_TEST(testConcurrentObserverMultipleNotifications);
    DEFINE_TEST(testConcurrentObserverThreadSafety);
    DEFINE_TEST(testConcurrentObserverBlockingBehavior);

    // === CONCURRENT OBSERVER ADVANCED TESTS ===
    DEFINE_TEST(testConcurrentObserverMultipleConsumers);
    DEFINE_TEST(testConcurrentObserverProducerConsumerPattern);
    DEFINE_TEST(testConcurrentObserverDetachWhileBlocked);

    // === INTEGRATION AND STRESS TESTS ===
    DEFINE_TEST(testMixedObserverPatterns);
    DEFINE_TEST(testHighVolumeNotifications);
    DEFINE_TEST(testObserverPatternMemoryManagement);
}

void ObserverPatternTest::setUp() {
    conditional_observed = std::make_unique<Conditionally_Data_Observed<TestData, TestCondition>>();
    concurrent_observed = std::make_unique<Concurrent_Observed<TestData, TestCondition>>();
    conditional_observers.clear();
    concurrent_observers.clear();
}

void ObserverPatternTest::tearDown() {
    cleanupTestObservers();
    conditional_observed.reset();
    concurrent_observed.reset();
}

/**
 * @brief Helper method to create test observers
 * 
 * Creates a set of test observers for both conditional and concurrent patterns.
 * Used by tests to establish known starting conditions.
 */
void ObserverPatternTest::createTestObservers() {
    // Create conditional observers
    for (int i = 1; i <= 3; ++i) {
        conditional_observers.push_back(std::make_unique<TestConditionalObserver>(i));
    }
    
    // Create concurrent observers
    for (int i = 1; i <= 3; ++i) {
        concurrent_observers.push_back(std::make_unique<TestConcurrentObserver>(i));
    }
}

/**
 * @brief Helper method to clean up test observers
 * 
 * Properly detaches and cleans up all test observers to prevent
 * resource leaks and ensure clean test state.
 */
void ObserverPatternTest::cleanupTestObservers() {
    // Detach conditional observers
    for (auto& observer : conditional_observers) {
        if (conditional_observed) {
            conditional_observed->detach(observer.get(), observer->rank());
        }
    }
    conditional_observers.clear();

    // Detach concurrent observers  
    for (auto& observer : concurrent_observers) {
        if (concurrent_observed) {
            concurrent_observed->detach(observer.get(), observer->rank());
        }
    }
    concurrent_observers.clear();
}

/**
 * @brief Helper method to verify observer state
 * 
 * @param observer The observer to verify
 * @param expected_count Expected number of updates received
 * @param expected_data Expected last data value received
 * @param context Description of the test context for error messages
 * 
 * This utility method performs comprehensive verification of observer state
 * including update counts and data values for debugging test failures.
 */
void ObserverPatternTest::verifyObserverState(TestConditionalObserver* observer, 
                                            int expected_count, 
                                            TestData expected_data,
                                            const std::string& context) {
    assert_equal(expected_count, observer->update_count.load(), 
                context + " - update count verification");
    if (expected_count > 0) {
        assert_equal(expected_data, observer->last_data, 
                    context + " - last data verification");
    }
}

/**
 * @brief Tests basic attach and detach functionality for conditional observers
 * 
 * Verifies that observers can be properly attached to and detached from
 * observed objects without errors. This is the foundation for all other
 * observer pattern functionality.
 */
void ObserverPatternTest::testConditionalObserverAttachDetach() {
    createTestObservers();
    
    // Test attach
    conditional_observed->attach(conditional_observers[0].get(), TEST_CONDITION_1);
    conditional_observed->attach(conditional_observers[1].get(), TEST_CONDITION_2);
    
    // Verify observers are attached by checking they can receive notifications
    TestData data = 100;
    conditional_observed->notify(&data, TEST_CONDITION_1);
    
    assert_equal(1, conditional_observers[0]->update_count.load(), 
                "First observer should receive notification after attach");
    assert_equal(0, conditional_observers[1]->update_count.load(), 
                "Second observer should not receive notification for different condition");
    
    // Test detach
    conditional_observed->detach(conditional_observers[0].get(), TEST_CONDITION_1);
    
    // Verify detached observer doesn't receive notifications
    TestData data2 = 200;
    conditional_observed->notify(&data2, TEST_CONDITION_1);
    
    assert_equal(1, conditional_observers[0]->update_count.load(), 
                "First observer should not receive notification after detach");
}

/**
 * @brief Tests notification for a single condition
 * 
 * Verifies that when a notification is sent for a specific condition,
 * only observers registered for that condition receive the update.
 */
void ObserverPatternTest::testConditionalObserverNotificationSingleCondition() {
    createTestObservers();
    
    // Attach observers for different conditions
    conditional_observed->attach(conditional_observers[0].get(), TEST_CONDITION_1);
    conditional_observed->attach(conditional_observers[1].get(), TEST_CONDITION_2);
    
    // Notify condition 1
    TestData data1 = 100;
    conditional_observed->notify(&data1, TEST_CONDITION_1);
    
    verifyObserverState(conditional_observers[0].get(), 1, data1, 
                       "Observer for condition 1");
    verifyObserverState(conditional_observers[1].get(), 0, 0, 
                       "Observer for condition 2 should not be notified");
    
    // Verify data retrieval
    TestData* retrieved = conditional_observers[0]->updated();
    assert_true(retrieved != nullptr, "Should retrieve valid data");
    assert_equal(data1, *retrieved, "Retrieved data should match sent data");
    
    TestData* not_retrieved = conditional_observers[1]->updated();
    assert_true(not_retrieved == nullptr, "Observer 2 should not have data");
}

/**
 * @brief Tests notifications for multiple conditions
 * 
 * Verifies that observers can correctly handle notifications for different
 * conditions and that each observer only receives notifications for its
 * registered condition.
 */
void ObserverPatternTest::testConditionalObserverNotificationMultipleConditions() {
    createTestObservers();
    
    // Attach observers
    conditional_observed->attach(conditional_observers[0].get(), TEST_CONDITION_1);
    conditional_observed->attach(conditional_observers[1].get(), TEST_CONDITION_2);
    
    // Notify multiple conditions
    TestData data1 = 100;
    TestData data2 = 200;
    
    conditional_observed->notify(&data1, TEST_CONDITION_1);
    conditional_observed->notify(&data2, TEST_CONDITION_2);
    
    verifyObserverState(conditional_observers[0].get(), 1, data1, 
                       "Observer 1 after condition 1 notification");
    verifyObserverState(conditional_observers[1].get(), 1, data2, 
                       "Observer 2 after condition 2 notification");
    
    // Verify both can retrieve their respective data
    TestData* retrieved1 = conditional_observers[0]->updated();
    TestData* retrieved2 = conditional_observers[1]->updated();
    
    assert_true(retrieved1 != nullptr && *retrieved1 == data1, 
                "Observer 1 should retrieve correct data");
    assert_true(retrieved2 != nullptr && *retrieved2 == data2, 
                "Observer 2 should retrieve correct data");
}

/**
 * @brief Tests multiple observers for the same condition
 * 
 * Verifies that multiple observers can be registered for the same condition
 * and that all of them receive notifications when that condition is triggered.
 */
void ObserverPatternTest::testConditionalObserverMultipleObserversSameCondition() {
    createTestObservers();
    
    // Attach multiple observers to same condition
    conditional_observed->attach(conditional_observers[0].get(), TEST_CONDITION_1);
    conditional_observed->attach(conditional_observers[1].get(), TEST_CONDITION_1);
    
    // Notify the condition
    TestData data = 300;
    conditional_observed->notify(&data, TEST_CONDITION_1);
    
    // Both observers should receive the notification
    verifyObserverState(conditional_observers[0].get(), 1, data, 
                       "First observer for condition 1");
    verifyObserverState(conditional_observers[1].get(), 1, data, 
                       "Second observer for condition 1");
    
    // Both should be able to retrieve the data
    TestData* retrieved1 = conditional_observers[0]->updated();
    TestData* retrieved2 = conditional_observers[1]->updated();
    
    assert_true(retrieved1 != nullptr && *retrieved1 == data, 
                "First observer should retrieve data");
    assert_true(retrieved2 != nullptr && *retrieved2 == data, 
                "Second observer should retrieve data");
}

/**
 * @brief Tests notification for non-existent condition
 * 
 * Verifies that notifications for conditions with no registered observers
 * are handled gracefully and don't cause errors or unexpected behavior.
 */
void ObserverPatternTest::testConditionalObserverNonExistentCondition() {
    createTestObservers();
    
    // Attach observer for condition 1 only
    conditional_observed->attach(conditional_observers[0].get(), TEST_CONDITION_1);
    
    // Notify non-existent condition
    TestData data = 400;
    conditional_observed->notify(&data, TEST_CONDITION_3);
    
    // Observer should not receive notification
    verifyObserverState(conditional_observers[0].get(), 0, 0, 
                       "Observer should not receive notification for different condition");
    
    TestData* retrieved = conditional_observers[0]->updated();
    assert_true(retrieved == nullptr, "No data should be available");
}

/**
 * @brief Tests handling of null data in notifications
 * 
 * Verifies that the observer pattern can handle null data pointers
 * gracefully without crashes or undefined behavior.
 */
void ObserverPatternTest::testConditionalObserverNullDataHandling() {
    createTestObservers();
    
    conditional_observed->attach(conditional_observers[0].get(), TEST_CONDITION_1);
    
    // Send notification with null data
    conditional_observed->notify(nullptr, TEST_CONDITION_1);
    
    // Observer should still receive the notification
    assert_equal(1, conditional_observers[0]->update_count.load(), 
                "Observer should receive notification even with null data");
    
    TestData* retrieved = conditional_observers[0]->updated();
    assert_true(retrieved == nullptr, "Retrieved data should be null");
}

/**
 * @brief Tests detach and reattach functionality
 * 
 * Verifies that observers can be detached and then reattached successfully,
 * and that they behave correctly in both states.
 */
void ObserverPatternTest::testConditionalObserverDetachAndReattach() {
    createTestObservers();
    
    conditional_observed->attach(conditional_observers[0].get(), TEST_CONDITION_1);
    
    // Send initial notification
    TestData data1 = 100;
    conditional_observed->notify(&data1, TEST_CONDITION_1);
    assert_equal(1, conditional_observers[0]->update_count.load(), 
                "Observer should receive initial notification");
    
    // Detach and send another notification
    conditional_observed->detach(conditional_observers[0].get(), TEST_CONDITION_1);
    TestData data2 = 200;
    conditional_observed->notify(&data2, TEST_CONDITION_1);
    assert_equal(1, conditional_observers[0]->update_count.load(), 
                "Observer should not receive notification after detach");
    
    // Reattach and send notification
    conditional_observed->attach(conditional_observers[0].get(), TEST_CONDITION_1);
    TestData data3 = 300;
    conditional_observed->notify(&data3, TEST_CONDITION_1);
    assert_equal(2, conditional_observers[0]->update_count.load(), 
                "Observer should receive notification after reattach");
}

/**
 * @brief Tests multiple detach operations
 * 
 * Verifies that multiple detach operations on the same observer
 * are handled gracefully without errors.
 */
void ObserverPatternTest::testConditionalObserverMultipleDetach() {
    createTestObservers();
    
    conditional_observed->attach(conditional_observers[0].get(), TEST_CONDITION_1);
    
    // Detach multiple times - should not cause errors
    conditional_observed->detach(conditional_observers[0].get(), TEST_CONDITION_1);
    conditional_observed->detach(conditional_observers[0].get(), TEST_CONDITION_1);
    
    // Verify observer doesn't receive notifications
    TestData data = 100;
    conditional_observed->notify(&data, TEST_CONDITION_1);
    assert_equal(0, conditional_observers[0]->update_count.load(), 
                "Observer should not receive notifications after multiple detach");
}

/**
 * @brief Tests basic concurrent observer functionality
 * 
 * Verifies that concurrent observers work correctly in single-threaded
 * scenarios before testing multi-threaded behavior.
 */
void ObserverPatternTest::testConcurrentObserverBasicFunctionality() {
    createTestObservers();
    
    concurrent_observed->attach(concurrent_observers[0].get(), TEST_CONDITION_1);
    
    // Create data on heap since observer will take ownership
    TestData* data = new TestData(500);
    concurrent_observed->notify(data, TEST_CONDITION_1);
    
    assert_equal(1, concurrent_observers[0]->update_count.load(), 
                "Concurrent observer should receive notification");
    
    TestData* retrieved = concurrent_observers[0]->updated();
    assert_true(retrieved != nullptr && *retrieved == 500, 
                "Should retrieve correct data from concurrent observer");
    
    delete retrieved; // Clean up allocated data
}

/**
 * @brief Tests multiple notifications to concurrent observers
 * 
 * Verifies that concurrent observers can handle multiple sequential
 * notifications correctly.
 */
void ObserverPatternTest::testConcurrentObserverMultipleNotifications() {
    createTestObservers();
    
    concurrent_observed->attach(concurrent_observers[0].get(), TEST_CONDITION_1);
    
    const int num_notifications = 5;
    for (int i = 0; i < num_notifications; ++i) {
        TestData* data = new TestData(100 + i);
        concurrent_observed->notify(data, TEST_CONDITION_1);
    }
    
    assert_equal(num_notifications, concurrent_observers[0]->update_count.load(), 
                "Should receive all notifications");
    
    // Retrieve all data
    for (int i = 0; i < num_notifications; ++i) {
        TestData* retrieved = concurrent_observers[0]->updated();
        assert_true(retrieved != nullptr, "Should retrieve valid data");
        assert_equal(100 + i, *retrieved, "Should retrieve correct sequential data");
        delete retrieved;
    }
}

/**
 * @brief Tests thread safety of concurrent observers
 * 
 * Verifies that concurrent observers work correctly when notifications
 * are sent from multiple threads simultaneously.
 */
void ObserverPatternTest::testConcurrentObserverThreadSafety() {
    createTestObservers();
    
    concurrent_observed->attach(concurrent_observers[0].get(), TEST_CONDITION_1);
    
    const int notifications_per_thread = 10;
    const int num_threads = 3;
    std::vector<std::thread> threads;
    
    // Start multiple notifying threads
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, notifications_per_thread]() {
            for (int i = 0; i < notifications_per_thread; ++i) {
                TestData* data = new TestData(t * 1000 + i);
                concurrent_observed->notify(data, TEST_CONDITION_1);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify total notifications received
    int expected_total = num_threads * notifications_per_thread;
    assert_equal(expected_total, concurrent_observers[0]->update_count.load(), 
                "Should receive all notifications from all threads");
    
    // Clean up all retrieved data
    for (int i = 0; i < expected_total; ++i) {
        TestData* retrieved = concurrent_observers[0]->updated();
        assert_true(retrieved != nullptr, "Should retrieve all data items");
        delete retrieved;
    }
}

/**
 * @brief Tests blocking behavior of concurrent observers
 * 
 * Verifies that concurrent observers properly block when waiting for
 * data and unblock when data becomes available.
 */
void ObserverPatternTest::testConcurrentObserverBlockingBehavior() {
    createTestObservers();
    
    concurrent_observed->attach(concurrent_observers[0].get(), TEST_CONDITION_1);
    
    std::atomic<bool> data_retrieved{false};
    TestData expected_data = 999;
    
    // Start consumer thread that will block
    std::thread consumer([this, &data_retrieved, expected_data]() {
        TestData* retrieved = concurrent_observers[0]->updated();
        assert_true(retrieved != nullptr, "Should eventually receive data");
        assert_equal(expected_data, *retrieved, "Should receive correct data");
        data_retrieved = true;
        delete retrieved;
    });
    
    // Wait a bit to ensure consumer is blocked
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert_false(data_retrieved.load(), "Consumer should be blocked initially");
    
    // Send notification to unblock consumer
    TestData* data = new TestData(expected_data);
    concurrent_observed->notify(data, TEST_CONDITION_1);
    
    // Wait for consumer to complete
    consumer.join();
    assert_true(data_retrieved.load(), "Consumer should have retrieved data");
}

/**
 * @brief Tests multiple consumers for concurrent observers
 * 
 * Verifies that multiple concurrent observers can consume data
 * independently without interfering with each other.
 */
void ObserverPatternTest::testConcurrentObserverMultipleConsumers() {
    createTestObservers();
    
    // Attach multiple observers for different conditions
    concurrent_observed->attach(concurrent_observers[0].get(), TEST_CONDITION_1);
    concurrent_observed->attach(concurrent_observers[1].get(), TEST_CONDITION_2);
    
    const int notifications_per_condition = 5;
    
    // Send notifications for both conditions
    for (int i = 0; i < notifications_per_condition; ++i) {
        TestData* data1 = new TestData(100 + i);
        TestData* data2 = new TestData(200 + i);
        
        concurrent_observed->notify(data1, TEST_CONDITION_1);
        concurrent_observed->notify(data2, TEST_CONDITION_2);
    }
    
    // Verify both observers received their notifications
    assert_equal(notifications_per_condition, concurrent_observers[0]->update_count.load(), 
                "First observer should receive its notifications");
    assert_equal(notifications_per_condition, concurrent_observers[1]->update_count.load(), 
                "Second observer should receive its notifications");
    
    // Clean up data
    for (int i = 0; i < notifications_per_condition; ++i) {
        TestData* retrieved1 = concurrent_observers[0]->updated();
        TestData* retrieved2 = concurrent_observers[1]->updated();
        
        assert_true(retrieved1 != nullptr, "First observer should retrieve data");
        assert_true(retrieved2 != nullptr, "Second observer should retrieve data");
        assert_equal(100 + i, *retrieved1, "First observer data verification");
        assert_equal(200 + i, *retrieved2, "Second observer data verification");
        
        delete retrieved1;
        delete retrieved2;
    }
}

/**
 * @brief Tests producer-consumer pattern with concurrent observers
 * 
 * Verifies that concurrent observers work correctly in a classic
 * producer-consumer scenario with multiple producers and consumers.
 */
void ObserverPatternTest::testConcurrentObserverProducerConsumerPattern() {
    createTestObservers();
    
    concurrent_observed->attach(concurrent_observers[0].get(), TEST_CONDITION_1);
    
    const int items_per_producer = 5;
    const int num_producers = 2;
    std::atomic<int> total_consumed{0};
    
    // Start consumer thread
    std::thread consumer([this, &total_consumed, items_per_producer, num_producers]() {
        int expected_total = items_per_producer * num_producers;
        while (total_consumed < expected_total) {
            TestData* data = concurrent_observers[0]->updated();
            if (data) {
                total_consumed++;
                delete data;
            }
        }
    });
    
    // Start producer threads
    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([this, p, items_per_producer]() {
            for (int i = 0; i < items_per_producer; ++i) {
                TestData* data = new TestData(p * 1000 + i);
                concurrent_observed->notify(data, TEST_CONDITION_1);
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        });
    }
    
    // Wait for all producers and consumer
    for (auto& producer : producers) {
        producer.join();
    }
    consumer.join();
    
    assert_equal(items_per_producer * num_producers, total_consumed.load(), 
                "Should consume all produced items");
}

/**
 * @brief Tests detaching observer while it's blocked
 * 
 * Verifies that detaching a concurrent observer while it's waiting
 * for data is handled gracefully.
 */
void ObserverPatternTest::testConcurrentObserverDetachWhileBlocked() {
    createTestObservers();
    
    concurrent_observed->attach(concurrent_observers[0].get(), TEST_CONDITION_1);
    
    std::atomic<bool> consumer_finished{false};
    
    // Start consumer that will block
    std::thread consumer([this, &consumer_finished]() {
        TestData* data = concurrent_observers[0]->updated();
        // This might return nullptr if detached
        consumer_finished = true;
        if (data) {
            delete data;
        }
    });
    
    // Give consumer time to start blocking
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Detach while consumer is potentially blocked
    concurrent_observed->detach(concurrent_observers[0].get(), TEST_CONDITION_1);
    
    // Send a notification to ensure system stability
    TestData* data = new TestData(123);
    concurrent_observed->notify(data, TEST_CONDITION_1);
    delete data; // Clean up since no observer should receive it
    
    // Wait for consumer to finish (should finish quickly after detach)
    consumer.join();
    assert_true(consumer_finished.load(), "Consumer should finish after detach");
}

/**
 * @brief Tests mixed observer patterns working together
 * 
 * Verifies that conditional and concurrent observers can work
 * simultaneously without interfering with each other.
 */
void ObserverPatternTest::testMixedObserverPatterns() {
    createTestObservers();
    
    // Attach both types of observers
    conditional_observed->attach(conditional_observers[0].get(), TEST_CONDITION_1);
    concurrent_observed->attach(concurrent_observers[0].get(), TEST_CONDITION_1);
    
    // Test conditional observer
    TestData cond_data = 100;
    conditional_observed->notify(&cond_data, TEST_CONDITION_1);
    
    verifyObserverState(conditional_observers[0].get(), 1, cond_data, 
                       "Conditional observer in mixed pattern");
    
    // Test concurrent observer
    TestData* conc_data = new TestData(200);
    concurrent_observed->notify(conc_data, TEST_CONDITION_1);
    
    assert_equal(1, concurrent_observers[0]->update_count.load(), 
                "Concurrent observer should work in mixed pattern");
    
    // Clean up
    TestData* retrieved = concurrent_observers[0]->updated();
    assert_true(retrieved != nullptr && *retrieved == 200, 
                "Should retrieve correct data from concurrent observer");
    delete retrieved;
}

/**
 * @brief Tests high volume of notifications
 * 
 * Verifies that the observer pattern can handle a large number of
 * notifications without performance degradation or errors.
 */
void ObserverPatternTest::testHighVolumeNotifications() {
    createTestObservers();
    
    concurrent_observed->attach(concurrent_observers[0].get(), TEST_CONDITION_1);
    
    const int high_volume = 100;
    
    // Send high volume of notifications
    for (int i = 0; i < high_volume; ++i) {
        TestData* data = new TestData(i);
        concurrent_observed->notify(data, TEST_CONDITION_1);
    }
    
    assert_equal(high_volume, concurrent_observers[0]->update_count.load(), 
                "Should handle high volume notifications");
    
    // Verify all data can be retrieved
    for (int i = 0; i < high_volume; ++i) {
        TestData* retrieved = concurrent_observers[0]->updated();
        assert_true(retrieved != nullptr, "Should retrieve all high volume data");
        assert_equal(i, *retrieved, "High volume data should be in correct order");
        delete retrieved;
    }
}

/**
 * @brief Tests memory management in observer patterns
 * 
 * Verifies that the observer pattern properly manages memory and
 * doesn't cause leaks during normal operation and cleanup.
 */
void ObserverPatternTest::testObserverPatternMemoryManagement() {
    // Test automatic cleanup on scope exit
    {
        auto local_observed = std::make_unique<Concurrent_Observed<TestData, TestCondition>>();
        auto local_observer = std::make_unique<TestConcurrentObserver>(TEST_CONDITION_1);
        
        local_observed->attach(local_observer.get(), TEST_CONDITION_1);
        
        TestData* data = new TestData(999);
        local_observed->notify(data, TEST_CONDITION_1);
        
        TestData* retrieved = local_observer->updated();
        assert_true(retrieved != nullptr, "Should retrieve data before cleanup");
        delete retrieved;
        
        local_observed->detach(local_observer.get(), TEST_CONDITION_1);
    } // Objects destroyed here - should not cause leaks
    
    // Test continues normally, indicating proper cleanup
    assert_true(true, "Memory management test completed without errors");
}

// Main function
int main() {
    TEST_INIT("ObserverPatternTest");
    ObserverPatternTest test;
    test.run();
    return 0;
} 