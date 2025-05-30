#include <iostream>
#include <vector>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <cassert>
#include <atomic>
#include <fstream>
#include <sstream>

// Include the test utilities
#include "../test_utils.h"

// Include the actual header files
#include "util/observer.h"
#include "util/observed.h"

// Global logger for this test
test::Logger* test_logger = nullptr;

// Define our own logging macros that use the global test_logger
#define OBS_TEST_LOG(message) \
    if (test_logger) { test_logger->log(message); }

#define OBS_TEST_ASSERT(condition, message) \
    if (test_logger) { test_logger->assertThat((condition), message); } \
    else { assert(condition); }

// Helper function to get thread id as string
std::string thread_id_to_string() {
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
}

// --- Test Data and Conditions ---
using TestData = int;
using TestCondition = int;

// --- Test Implementations ---

// Test Observer for Conditional Pattern
class MyConditionalObserver : public Conditional_Data_Observer<TestData, TestCondition> {
public:
    std::atomic<int> update_count{0}; // Count how many times update was called

    MyConditionalObserver(TestCondition c) : Conditional_Data_Observer<TestData, TestCondition>(c) {}

    void update(TestCondition c, TestData* d) override {
        if (test_logger) {
            test_logger->log("MyConditionalObserver (Rank " + std::to_string(this->_rank) + 
                            "): Received update for condition " + std::to_string(c) + 
                            " with data " + std::to_string(d ? *d : 0));
        }
        Conditional_Data_Observer<TestData, TestCondition>::update(c, d); // Call base implementation
        if (c == _rank) {
            update_count++;
        }
    }
};

// Test Observer for Concurrent Pattern
class MyConcurrentObserver : public Concurrent_Observer<TestData, TestCondition> {
public:
    std::atomic<int> update_count{0}; // Count how many times update was called (and semaphore posted)
    std::atomic<int> retrieved_count{0}; // Count how many items were retrieved via updated()

    MyConcurrentObserver(TestCondition c) : Concurrent_Observer<TestData, TestCondition>(c) {}

    // Override update just for logging, still call base for semaphore logic
    void update(TestCondition c, TestData* d) override {
        if (test_logger) {
            test_logger->log("MyConcurrentObserver (Rank " + std::to_string(this->_rank) + 
                            ", Thread " + thread_id_to_string() + 
                            "): Received update for condition " + std::to_string(c) + 
                            " with data " + std::to_string(d ? *d : 0));
        }
        if (c == this->_rank && d != nullptr) {
            update_count++;
        }
        Concurrent_Observer<TestData, TestCondition>::update(c, d); // IMPORTANT: Call base class update
    }

    TestData* updated() {
        if (test_logger) {
            test_logger->log("MyConcurrentObserver (Rank " + std::to_string(this->_rank) + 
                            ", Thread " + thread_id_to_string() + 
                            "): Waiting for data...");
        }
        TestData* data = Concurrent_Observer<TestData, TestCondition>::updated(); // IMPORTANT: Call base class updated
        if (data) {
            retrieved_count++;
            if (test_logger) {
                test_logger->log("MyConcurrentObserver (Rank " + std::to_string(this->_rank) + 
                                ", Thread " + thread_id_to_string() + 
                                "): Retrieved data " + std::to_string(*data));
            }
        } else {
            if (test_logger) {
                test_logger->log("MyConcurrentObserver (Rank " + std::to_string(this->_rank) + 
                                ", Thread " + thread_id_to_string() + 
                                "): Retrieved nullptr");
            }
        }
        return data;
    }
};

// --- Test Functions ---

void test_conditional_observer() {
    OBS_TEST_LOG("\n--- Testing Conditional Observer Pattern ---\n");

    Conditionally_Data_Observed<TestData, TestCondition> observed;
    MyConditionalObserver observer1(1); // Interested in condition 1
    MyConditionalObserver observer2(2); // Interested in condition 2
    MyConditionalObserver observer1_too(1); // Also interested in condition 1

    TestData data1 = 100;
    TestData data2 = 200;
    TestData data3 = 300;

    // Attach observers
    observed.attach(&observer1, observer1.rank());
    observed.attach(&observer2, observer2.rank());
    observed.attach(&observer1_too, observer1_too.rank());
    OBS_TEST_LOG("Attached observers.");

    // Notify for condition 1
    OBS_TEST_LOG("Notifying condition 1...");
    bool notified1 = observed.notify(1, &data1);
    OBS_TEST_ASSERT(notified1, "Should notify someone for condition 1");
    OBS_TEST_ASSERT(observer1.update_count == 1, "Observer1 should be updated once");
    OBS_TEST_ASSERT(observer1_too.update_count == 1, "Observer1_too should be updated once");
    OBS_TEST_ASSERT(observer2.update_count == 0, "Observer2 should not be notified for condition 1");

    // Retrieve data
    TestData* retrieved1 = observer1.updated();
    TestData* retrieved1_too = observer1_too.updated();
    TestData* retrieved2 = observer2.updated();
    OBS_TEST_ASSERT(retrieved1 != nullptr && *retrieved1 == data1, "Observer1 should retrieve correct data");
    OBS_TEST_ASSERT(retrieved1_too != nullptr && *retrieved1_too == data1, "Observer1_too should retrieve correct data");
    OBS_TEST_ASSERT(retrieved2 == nullptr, "Observer2 should not retrieve any data");
    OBS_TEST_LOG("Retrieved data after notify 1.");

    // Notify for condition 2
    OBS_TEST_LOG("Notifying condition 2...");
    bool notified2 = observed.notify(2, &data2);
    OBS_TEST_ASSERT(notified2, "Should notify someone for condition 2");
    OBS_TEST_ASSERT(observer1.update_count == 1, "Observer1 count shouldn't change for condition 2");
    OBS_TEST_ASSERT(observer1_too.update_count == 1, "Observer1_too count shouldn't change for condition 2");
    OBS_TEST_ASSERT(observer2.update_count == 1, "Observer2 should be notified once for condition 2");

    retrieved1 = observer1.updated();
    retrieved1_too = observer1_too.updated();
    retrieved2 = observer2.updated();
    OBS_TEST_ASSERT(retrieved1 == nullptr, "Observer1 should not receive new data");
    OBS_TEST_ASSERT(retrieved1_too == nullptr, "Observer1_too should not receive new data");
    OBS_TEST_ASSERT(retrieved2 != nullptr && *retrieved2 == data2, "Observer2 should get correct data");
    OBS_TEST_LOG("Retrieved data after notify 2.");

     // Notify for condition 3 (no observers)
    OBS_TEST_LOG("Notifying condition 3...");
    bool notified3 = observed.notify(3, &data3);
    OBS_TEST_ASSERT(!notified3, "No observers for condition 3, should not notify");
    OBS_TEST_ASSERT(observer1.update_count == 1, "Observer1 count shouldn't change for condition 3");
    OBS_TEST_ASSERT(observer1_too.update_count == 1, "Observer1_too count shouldn't change for condition 3");
    OBS_TEST_ASSERT(observer2.update_count == 1, "Observer2 count shouldn't change for condition 3");
    OBS_TEST_LOG("Verified no notification for condition 3.");

    // Detach observer1
    OBS_TEST_LOG("Detaching observer1...");
    observed.detach(&observer1, observer1.rank());

    // Notify for condition 1 again
    OBS_TEST_LOG("Notifying condition 1 again...");
    bool notified4 = observed.notify(1, &data1); // Use same data address for simplicity
    OBS_TEST_ASSERT(notified4, "Observer1_too is still attached, should notify");
    OBS_TEST_ASSERT(observer1.update_count == 1, "Observer1 should not have been updated after detach");
    OBS_TEST_ASSERT(observer1_too.update_count == 2, "Observer1_too should be updated again");
    OBS_TEST_ASSERT(observer2.update_count == 1, "Observer2 count shouldn't change for condition 1");

    retrieved1 = observer1.updated();
    retrieved1_too = observer1_too.updated();
    OBS_TEST_ASSERT(retrieved1 == nullptr, "Observer1 should not receive data after detach");
    OBS_TEST_ASSERT(retrieved1_too != nullptr && *retrieved1_too == data1, "Observer1_too should receive correct data");
    OBS_TEST_LOG("Verified observer1 didn't update after detach.");

    OBS_TEST_LOG("--- Conditional Observer Test Passed ---");
}


// --- Concurrent Test Setup ---
Concurrent_Observed<TestData, TestCondition> concurrent_observed;
MyConcurrentObserver concurrent_observer1(1);
MyConcurrentObserver concurrent_observer2(2);

std::atomic<bool> keep_running{true};
std::vector<TestData> produced_data; // Store pointers to allocated data
std::mutex data_mutex; // Protect access to produced_data vector

// Thread function for notifying
void notify_thread_func(TestCondition condition, int start_value, int count) {
    if (test_logger) {
        test_logger->log("Notify Thread (Condition " + std::to_string(condition) + 
                         ", Thread " + thread_id_to_string() + ") started.");
    }
    for (int i = 0; i < count && keep_running; ++i) {
        TestData* data = new TestData(start_value + i); // Allocate data
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            produced_data.push_back(*data); // Store value for later check
        }

        if (test_logger) {
            test_logger->log("Notify Thread (Condition " + std::to_string(condition) + 
                             ", Thread " + thread_id_to_string() + 
                             ") notifying with data " + std::to_string(*data));
        }
        concurrent_observed.notify(condition, data);

        // Simulate some work
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (test_logger) {
        test_logger->log("Notify Thread (Condition " + std::to_string(condition) + 
                         ", Thread " + thread_id_to_string() + ") finished.");
    }
}

// Thread function for consuming updates
void consume_thread_func(MyConcurrentObserver* observer, int expected_count) {
    if (test_logger) {
        test_logger->log("Consume Thread (Observer Rank " + std::to_string(observer->rank()) + 
                         ", Thread " + thread_id_to_string() + ") started.");
    }
    int consumed_count = 0;
    while(consumed_count < expected_count && keep_running) {
        TestData* data = observer->updated(); // This will block until semaphore is posted
        if (data) {
            consumed_count++;
            if (test_logger) {
                test_logger->log("Consume Thread (Observer Rank " + std::to_string(observer->rank()) + 
                                 ", Thread " + thread_id_to_string() + 
                                 ") consumed data " + std::to_string(*data) + " (" + 
                                 std::to_string(consumed_count) + "/" + std::to_string(expected_count) + ")");
            }
            // In a real app, process the data. Here we just delete it.
            delete data; // Clean up allocated data
        } else {
            if (test_logger) {
                test_logger->log("Consume Thread (Observer Rank " + std::to_string(observer->rank()) + 
                                 ", Thread " + thread_id_to_string() + 
                                 ") received nullptr (maybe end signal?)");
            }
        }
    }
    if (test_logger) {
        test_logger->log("Consume Thread (Observer Rank " + std::to_string(observer->rank()) + 
                         ", Thread " + thread_id_to_string() + 
                         ") finished after consuming " + std::to_string(consumed_count) + " items.");
    }
    // Can't use TEST_ASSERT here since it's in a separate thread without access to the logger
    assert(consumed_count >= expected_count); // Ensure we consumed enough items
}


void test_concurrent_observer() {
    OBS_TEST_LOG("\n--- Testing Concurrent Observer Pattern ---\n");

    // Attach observers (using the thread-safe attach method)
    concurrent_observed.attach(&concurrent_observer1, concurrent_observer1.rank());
    concurrent_observed.attach(&concurrent_observer2, concurrent_observer2.rank());
    OBS_TEST_LOG("Attached concurrent observers.");

    const int num_notifications1 = 50;
    const int num_notifications2 = 40;

    // Create threads
    std::thread notifier1(notify_thread_func, 1, 1000, num_notifications1); // Condition 1, data starts at 1000
    std::thread notifier2(notify_thread_func, 2, 2000, num_notifications2); // Condition 2, data starts at 2000
    std::thread consumer1(consume_thread_func, &concurrent_observer1, num_notifications1);
    std::thread consumer2(consume_thread_func, &concurrent_observer2, num_notifications2);

    // Let threads run for a while
    OBS_TEST_LOG("Threads started, waiting for completion...");
    // In a real test, might wait for a specific duration or signal
    notifier1.join();
    notifier2.join();
    OBS_TEST_LOG("Notifier threads joined.");

    // Signal consumers to stop if they haven't finished (e.g., if notify failed)
    // This might be needed if notify isn't working as expected.
    // Alternatively, wait for consumers to naturally finish.
    consumer1.join();
    consumer2.join();
    OBS_TEST_LOG("Consumer threads joined.");

    keep_running = false; // Signal any remaining loops to stop (though join should suffice)

    // Verification
    OBS_TEST_LOG("Verifying results...");
    OBS_TEST_LOG("Observer 1: Updates Received = " + std::to_string(concurrent_observer1.update_count) + 
             ", Data Retrieved = " + std::to_string(concurrent_observer1.retrieved_count));
    OBS_TEST_LOG("Observer 2: Updates Received = " + std::to_string(concurrent_observer2.update_count) + 
             ", Data Retrieved = " + std::to_string(concurrent_observer2.retrieved_count));

    // Assertions (check if counts match expected notifications)
    OBS_TEST_ASSERT(concurrent_observer1.update_count == num_notifications1, 
                "Observer 1 should receive the expected number of updates");
    OBS_TEST_ASSERT(concurrent_observer1.retrieved_count == num_notifications1, 
                "Observer 1 should retrieve the expected amount of data");
    OBS_TEST_ASSERT(concurrent_observer2.update_count == num_notifications2, 
                "Observer 2 should receive the expected number of updates");
    OBS_TEST_ASSERT(concurrent_observer2.retrieved_count == num_notifications2, 
                "Observer 2 should retrieve the expected amount of data");

    // Check if all produced data values were roughly correct (simple check)
    {
        std::lock_guard<std::mutex> lock(data_mutex);
        OBS_TEST_ASSERT(produced_data.size() == num_notifications1 + num_notifications2, 
                    "Should produce the expected total number of data items");
        OBS_TEST_LOG("Total data items produced (and hopefully consumed/deleted): " + std::to_string(produced_data.size()));
    }

    // Test detach (basic test after main concurrency)
    OBS_TEST_LOG("Testing detach...");
    concurrent_observed.detach(&concurrent_observer1, concurrent_observer1.rank());
    TestData final_data = 9999;
    concurrent_observed.notify(1, &final_data); // Should not reach observer 1
    // Give a tiny moment for potential processing (though it shouldn't happen)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    OBS_TEST_ASSERT(concurrent_observer1.update_count == num_notifications1, 
                "Observer 1 count should not increase after detach");

    OBS_TEST_LOG("--- Concurrent Observer Test Finished ---");
}


int main() {
    // Initialize test using test_utils
    TEST_INIT("observer_pattern_test");
    
    // Set the global logger pointer
    test_logger = &logger;
    
    // Log the start using our custom logger wrapper
    OBS_TEST_LOG("Starting Observer Pattern Tests");
    
    // Run tests
    test_conditional_observer();
    test_concurrent_observer();
    
    OBS_TEST_LOG("All tests finished.");
    
    std::cout << "Observer pattern test passed successfully!" << std::endl;
    return 0;
} 