#include "observer.h"
#include "observed.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <cassert>
#include <atomic>

// Thread-safe output helper
class ThreadSafeOutput {
public:
    static void print(const std::string& msg) {
        std::lock_guard<std::mutex> lock(_mutex);
        std::cout << msg << std::endl;
    }
    
private:
    static std::mutex _mutex;
};

std::mutex ThreadSafeOutput::_mutex;
std::mutex test_mutex;

// Test data structures
struct TestData {
    int value;
    std::atomic<int> ref_count; // Reference counting
    
    TestData(int v) : value(v), ref_count(0) {}
};

enum class TestCondition {
    CONDITION_1,
    CONDITION_2,
    CONDITION_3
};

// Test observer implementation
class TestObserver : public Concurrent_Observer<TestData, TestCondition> {
public:
    TestObserver(TestCondition condition, const std::string& name)
        : Concurrent_Observer<TestData, TestCondition>(condition), _name(name), _running(true) {}

    void run() {
        ThreadSafeOutput::print(_name + " started waiting for data...");
        while (_running) {
            std::lock_guard<std::mutex> lock(test_mutex);
            TestData* data = updated();  // Will block until data is available
            if (!data) {
                // Null data could indicate shutdown
                continue;
            }
            
            if (data->value < 0) {
                ThreadSafeOutput::print(_name + " received termination signal");
                _running = false;
                
                // Decrement reference and delete if no more references
                if (--data->ref_count <= 0) {
                    delete data;
                }
                break;
            }
            
            ThreadSafeOutput::print(_name + " received value: " + std::to_string(data->value));
            
            // Decrement reference and delete if no more references
            if (--data->ref_count <= 0) {
                delete data;
            }
        }
    }
    
    void stop() {
        _running = false;
    }

private:
    std::string _name;
    std::atomic<bool> _running;
};

// Test observed implementation
class TestObserved : public Concurrent_Observed<TestData, TestCondition> {
public:
    void generateData(TestCondition condition, int value) {
        TestData* data = new TestData(value);
        
        // Reference count will be set in notify if there are observers
        
        // Notify observers and let the base class handle it
        // The return value tells us if any observers were notified
        bool notified = notify(condition, data);
        
        // If no observers were notified, delete the data
        if (!notified) {
            delete data;
        }
    }
};

// Test function to validate basic functionality
void test_basic_functionality() {
    ThreadSafeOutput::print("\nTesting basic functionality...");
    
    TestObserved observed;
    TestObserver observer1(TestCondition::CONDITION_1, "Observer1");
    TestObserver observer2(TestCondition::CONDITION_2, "Observer2");
    
    observed.attach(&observer1, TestCondition::CONDITION_1);
    observed.attach(&observer2, TestCondition::CONDITION_2);
    
    // Start observer threads
    std::thread t1(&TestObserver::run, &observer1);
    std::thread t2(&TestObserver::run, &observer2);
    
    // Generate some test data
    for (int i = 1; i <= 5; i++) {
        observed.generateData(TestCondition::CONDITION_1, i);
        observed.generateData(TestCondition::CONDITION_2, i * 10);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Send termination signal
    observed.generateData(TestCondition::CONDITION_1, -1);
    observed.generateData(TestCondition::CONDITION_2, -1);
    
    // Wait for threads to finish
    t1.join();
    t2.join();
    
    ThreadSafeOutput::print("Basic functionality test completed successfully");
}

// Test function to validate concurrent access
void test_concurrent_access() {
    ThreadSafeOutput::print("\nTesting concurrent access...");
    
    TestObserved observed;
    std::vector<std::unique_ptr<TestObserver>> observers;
    std::vector<std::thread> threads;
    
    // Create multiple observers for each condition
    for (int i = 0; i < 3; i++) {
        for (auto condition : {TestCondition::CONDITION_1, 
                             TestCondition::CONDITION_2, 
                             TestCondition::CONDITION_3}) {
            auto observer = std::make_unique<TestObserver>(
                condition, 
                "Observer_" + std::to_string(i) + "_" + std::to_string(static_cast<int>(condition))
            );
            observed.attach(observer.get(), condition);
            observers.push_back(std::move(observer));
        }
    }
    
    // Start all observer threads after attaching them all
    for (auto& observer : observers) {
        threads.emplace_back(&TestObserver::run, observer.get());
    }
    
    // Wait a bit to ensure all observers are ready
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Generate data from multiple threads
    std::vector<std::thread> producer_threads;
    for (int i = 0; i < 3; i++) {
        producer_threads.emplace_back([&observed, i]() {
            for (int j = 1; j <= 3; j++) {
                observed.generateData(TestCondition::CONDITION_1, i * 100 + j);
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                
                observed.generateData(TestCondition::CONDITION_2, i * 100 + j);
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                
                observed.generateData(TestCondition::CONDITION_3, i * 100 + j);
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        });
    }
    
    // Wait for producers to finish
    for (auto& t : producer_threads) {
        t.join();
    }
    
    // Give some time for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    // Send termination signal to all observers
    for (auto condition : {TestCondition::CONDITION_1, 
                          TestCondition::CONDITION_2, 
                          TestCondition::CONDITION_3}) {
        observed.generateData(condition, -1);
    }
    
    // Wait for all observers to finish
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    
    ThreadSafeOutput::print("Concurrent access test completed successfully");
}

int main() {
    try {
        test_basic_functionality();
        test_concurrent_access();
        ThreadSafeOutput::print("\nAll tests completed successfully!");
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Test failed with error: " << e.what() << std::endl;
        return 1;
    }
}