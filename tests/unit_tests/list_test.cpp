#include <iostream>
#include <thread>
#include <vector>
#include "../test_utils.h"
#include "util/list.h"

// Test class for List
class TestItem {
public:
    explicit TestItem(int val) : value(val) {}
    int getValue() const { return value; }
private:
    int value;
};

// Test class for Ordered_List with a rank
class RankedItem {
public:
    explicit RankedItem(int val, int r) : value(val), rank(r) {}
    int getValue() const { return value; }
    int getRank() const { return rank; }
private:
    int value;
    int rank;
};

// Function for testing thread safety
void listConcurrentTest(List<TestItem>* list, int start, int count, bool* success) {
    try {
        // Insert items
        std::vector<TestItem*> items;
        for (int i = 0; i < count; i++) {
            items.push_back(new TestItem(start + i));
        }
        
        for (auto item : items) {
            list->insert(item);
        }
        
        // Small delay to ensure interleaving of operations
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        *success = true;
    } catch (const std::exception& e) {
        *success = false;
    }
}

int main() {
    TEST_INIT("list_test");
    
    // Part 1: Test basic List operations
    {
        List<TestItem> list;
        
        // Test 1: Empty list
        TEST_ASSERT(list.empty(), "New list should be empty");
        
        // Test 2: Insert and retrieve items
        TestItem* item1 = new TestItem(1);
        TestItem* item2 = new TestItem(2);
        TestItem* item3 = new TestItem(3);
        
        list.insert(item1);
        TEST_ASSERT(!list.empty(), "List should not be empty after insert");
        
        list.insert(item2);
        list.insert(item3);
        
        // Test 3: Remove items in FIFO order
        TestItem* retrieved1 = list.remove();
        TEST_ASSERT(retrieved1 != nullptr, "Retrieved item should not be null");
        TEST_ASSERT(retrieved1->getValue() == 1, "First item should have value 1");
        
        TestItem* retrieved2 = list.remove();
        TEST_ASSERT(retrieved2 != nullptr, "Retrieved item should not be null");
        TEST_ASSERT(retrieved2->getValue() == 2, "Second item should have value 2");
        
        TestItem* retrieved3 = list.remove();
        TEST_ASSERT(retrieved3 != nullptr, "Retrieved item should not be null");
        TEST_ASSERT(retrieved3->getValue() == 3, "Third item should have value 3");
        
        // Test 4: Empty after removing all items
        TEST_ASSERT(list.empty(), "List should be empty after removing all items");
        
        // Test 5: Remove from empty list
        TestItem* nullItem = list.remove();
        TEST_ASSERT(nullItem == nullptr, "Removing from empty list should return nullptr");
        
        // Clean up
        delete item1;
        delete item2;
        delete item3;
    }
    
    // Part 2: Test Ordered_List operations
    {
        Ordered_List<RankedItem, int> orderedList;
        
        // Test 1: Empty list
        TEST_ASSERT(orderedList.empty(), "New ordered list should be empty");
        
        // Test 2: Insert items
        RankedItem* item1 = new RankedItem(1, 10);
        RankedItem* item2 = new RankedItem(2, 20);
        RankedItem* item3 = new RankedItem(3, 30);
        
        orderedList.insert(item1);
        TEST_ASSERT(!orderedList.empty(), "List should not be empty after insert");
        
        orderedList.insert(item2);
        orderedList.insert(item3);
        
        // Test 3: Iterate through items
        int count = 0;
        for (auto it = orderedList.begin(); it != orderedList.end(); ++it) {
            count++;
            TEST_ASSERT((*it)->getValue() == count, "Item value should match expected sequence");
        }
        TEST_ASSERT(count == 3, "Iterator should traverse 3 items");
        
        // Test 4: Remove an item
        orderedList.remove(item2);
        
        // Test 5: Check items after removal
        count = 0;
        int expectedValues[] = {1, 3};
        for (auto it = orderedList.begin(); it != orderedList.end(); ++it) {
            TEST_ASSERT((*it)->getValue() == expectedValues[count], "Item value should match expected after removal");
            count++;
        }
        TEST_ASSERT(count == 2, "Iterator should traverse 2 items after removal");
        
        // Clean up
        delete item1;
        delete item2;
        delete item3;
    }
    
    // Part 3: Test thread safety
    {
        List<TestItem> threadSafeList;
        const int NUM_THREADS = 5;
        const int ITEMS_PER_THREAD = 100;
        
        std::vector<std::thread> threads;
        bool threadSuccess[NUM_THREADS];
        
        // Create threads to insert items concurrently
        for (int i = 0; i < NUM_THREADS; i++) {
            threads.push_back(std::thread(
                listConcurrentTest, &threadSafeList, i * ITEMS_PER_THREAD, ITEMS_PER_THREAD, &threadSuccess[i]
            ));
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        // Check all threads succeeded
        bool allThreadsSucceeded = true;
        for (int i = 0; i < NUM_THREADS; i++) {
            if (!threadSuccess[i]) {
                allThreadsSucceeded = false;
                break;
            }
        }
        TEST_ASSERT(allThreadsSucceeded, "All threads should complete successfully");
        
        // Check that we can retrieve all items
        int itemCount = 0;
        while (!threadSafeList.empty()) {
            TestItem* item = threadSafeList.remove();
            if (item) {
                itemCount++;
                delete item;
            }
        }
        TEST_ASSERT(itemCount == NUM_THREADS * ITEMS_PER_THREAD, 
                    "Should retrieve all items inserted by threads");
    }
    
    std::cout << "List test passed successfully!" << std::endl;
    return 0;
} 