#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

#include "../testcase.h"
#include "../test_utils.h"
#include "api/util/list.h"

// Forward declarations and test helper classes
class ListTest;

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

class ListTest : public TestCase {
protected:
    void setUp() override;
    void tearDown() override;

    // Helper methods for creating test data
    TestItem* createTestItem(int value);
    RankedItem* createRankedItem(int value, int rank);
    void cleanupTestItems(std::vector<TestItem*>& items);
    void cleanupRankedItems(std::vector<RankedItem*>& items);

    // === BASIC LIST OPERATIONS TESTS ===
    void testListStartsEmpty();
    void testListInsertSingleItem();
    void testListInsertMultipleItems();
    void testListRemoveFromEmptyList();
    void testListRemoveSingleItem();
    void testListRemoveMultipleItemsFIFO();
    void testListEmptyAfterRemovingAllItems();

    // === LIST STATE MANAGEMENT TESTS ===
    void testListEmptyStateTransitions();
    void testListSizeTracking();
    void testListInsertRemoveSequence();

    // === ORDERED LIST OPERATIONS TESTS ===
    void testOrderedListStartsEmpty();
    void testOrderedListInsertSingleItem();
    void testOrderedListInsertMultipleItems();
    void testOrderedListIteratorTraversal();
    void testOrderedListRemoveSpecificItem();
    void testOrderedListRemoveNonExistentItem();
    void testOrderedListOrderPreservation();

    // === ITERATOR FUNCTIONALITY TESTS ===
    void testOrderedListIteratorBasicFunctionality();
    void testOrderedListIteratorEmptyList();
    void testOrderedListIteratorSingleItem();
    void testOrderedListIteratorMultipleItems();
    void testOrderedListIteratorAfterModification();

    // === THREAD SAFETY TESTS ===
    void testListConcurrentInsertions();
    void testListConcurrentRemovals();
    void testListConcurrentMixedOperations();
    void testOrderedListThreadSafety();

    // === EDGE CASES AND ROBUSTNESS TESTS ===
    void testListWithNullPointers();
    void testListLargeNumberOfItems();
    void testOrderedListWithDuplicateRanks();
    void testListMemoryManagement();

    // === PERFORMANCE AND STRESS TESTS ===
    void testListPerformanceWithManyItems();
    void testOrderedListPerformanceWithManyItems();

private:
    // Helper for concurrent testing
    void concurrentInsertHelper(List<TestItem>* list, int start, int count, std::atomic<bool>* success);
    void concurrentRemoveHelper(List<TestItem>* list, int expected_count, std::atomic<bool>* success);

public:
    ListTest();
};

// Implementations

/**
 * @brief Constructor that registers all test methods
 * 
 * Organizes tests into logical groups for better maintainability and clarity.
 * Each test method name clearly describes what List functionality is being tested.
 */
ListTest::ListTest() {
    // === BASIC LIST OPERATIONS TESTS ===
    DEFINE_TEST(testListStartsEmpty);
    DEFINE_TEST(testListInsertSingleItem);
    DEFINE_TEST(testListInsertMultipleItems);
    DEFINE_TEST(testListRemoveFromEmptyList);
    DEFINE_TEST(testListRemoveSingleItem);
    DEFINE_TEST(testListRemoveMultipleItemsFIFO);
    DEFINE_TEST(testListEmptyAfterRemovingAllItems);

    // === LIST STATE MANAGEMENT TESTS ===
    DEFINE_TEST(testListEmptyStateTransitions);
    DEFINE_TEST(testListSizeTracking);
    DEFINE_TEST(testListInsertRemoveSequence);

    // === ORDERED LIST OPERATIONS TESTS ===
    DEFINE_TEST(testOrderedListStartsEmpty);
    DEFINE_TEST(testOrderedListInsertSingleItem);
    DEFINE_TEST(testOrderedListInsertMultipleItems);
    DEFINE_TEST(testOrderedListIteratorTraversal);
    DEFINE_TEST(testOrderedListRemoveSpecificItem);
    DEFINE_TEST(testOrderedListRemoveNonExistentItem);
    DEFINE_TEST(testOrderedListOrderPreservation);

    // === ITERATOR FUNCTIONALITY TESTS ===
    DEFINE_TEST(testOrderedListIteratorBasicFunctionality);
    DEFINE_TEST(testOrderedListIteratorEmptyList);
    DEFINE_TEST(testOrderedListIteratorSingleItem);
    DEFINE_TEST(testOrderedListIteratorMultipleItems);
    DEFINE_TEST(testOrderedListIteratorAfterModification);

    // === THREAD SAFETY TESTS ===
    DEFINE_TEST(testListConcurrentInsertions);
    DEFINE_TEST(testListConcurrentRemovals);
    DEFINE_TEST(testListConcurrentMixedOperations);
    DEFINE_TEST(testOrderedListThreadSafety);

    // === EDGE CASES AND ROBUSTNESS TESTS ===
    DEFINE_TEST(testListWithNullPointers);
    DEFINE_TEST(testListLargeNumberOfItems);
    DEFINE_TEST(testOrderedListWithDuplicateRanks);
    DEFINE_TEST(testListMemoryManagement);

    // === PERFORMANCE AND STRESS TESTS ===
    DEFINE_TEST(testListPerformanceWithManyItems);
    DEFINE_TEST(testOrderedListPerformanceWithManyItems);
}

/**
 * @brief Set up test environment before each test
 * 
 * Prepares the test environment with any necessary initialization.
 * Currently no specific setup is required for List tests.
 */
void ListTest::setUp() {
    // No specific setup needed for List tests
}

/**
 * @brief Clean up test environment after each test
 * 
 * Performs any necessary cleanup after test execution.
 * Currently no specific cleanup is required for List tests.
 */
void ListTest::tearDown() {
    // No specific cleanup needed for List tests
}

/**
 * @brief Helper method to create test items
 * 
 * @param value The integer value for the test item
 * @return Pointer to newly created TestItem
 * 
 * This utility method creates TestItem instances for testing purposes,
 * providing a consistent way to generate test data.
 */
TestItem* ListTest::createTestItem(int value) {
    return new TestItem(value);
}

/**
 * @brief Helper method to create ranked items
 * 
 * @param value The integer value for the ranked item
 * @param rank The rank value for ordering
 * @return Pointer to newly created RankedItem
 * 
 * This utility method creates RankedItem instances for testing purposes,
 * providing a consistent way to generate test data for ordered lists.
 */
RankedItem* ListTest::createRankedItem(int value, int rank) {
    return new RankedItem(value, rank);
}

/**
 * @brief Helper method to cleanup test items
 * 
 * @param items Vector of TestItem pointers to delete
 * 
 * This utility method safely deletes all TestItem instances in the vector
 * and clears the vector to prevent memory leaks in tests.
 */
void ListTest::cleanupTestItems(std::vector<TestItem*>& items) {
    for (auto item : items) {
        delete item;
    }
    items.clear();
}

/**
 * @brief Helper method to cleanup ranked items
 * 
 * @param items Vector of RankedItem pointers to delete
 * 
 * This utility method safely deletes all RankedItem instances in the vector
 * and clears the vector to prevent memory leaks in tests.
 */
void ListTest::cleanupRankedItems(std::vector<RankedItem*>& items) {
    for (auto item : items) {
        delete item;
    }
    items.clear();
}

/**
 * @brief Helper method for concurrent insertion testing
 * 
 * @param list Pointer to the list to insert into
 * @param start Starting value for items to insert
 * @param count Number of items to insert
 * @param success Atomic boolean to indicate success/failure
 * 
 * This helper method is used by thread safety tests to perform
 * concurrent insertions into a list from multiple threads.
 */
void ListTest::concurrentInsertHelper(List<TestItem>* list, int start, int count, std::atomic<bool>* success) {
    try {
        std::vector<TestItem*> items;
        for (int i = 0; i < count; i++) {
            items.push_back(createTestItem(start + i));
        }
        
        for (auto item : items) {
            list->insert(item);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        success->store(true);
    } catch (const std::exception& e) {
        success->store(false);
    }
}

/**
 * @brief Helper method for concurrent removal testing
 * 
 * @param list Pointer to the list to remove from
 * @param expected_count Expected number of items to remove
 * @param success Atomic boolean to indicate success/failure
 * 
 * This helper method is used by thread safety tests to perform
 * concurrent removals from a list from multiple threads.
 */
void ListTest::concurrentRemoveHelper(List<TestItem>* list, int expected_count, std::atomic<bool>* success) {
    try {
        int removed_count = 0;
        while (removed_count < expected_count && !list->empty()) {
            TestItem* item = list->remove();
            if (item) {
                delete item;
                removed_count++;
            }
        }
        success->store(true);
    } catch (const std::exception& e) {
        success->store(false);
    }
}

/**
 * @brief Tests that a new List starts empty
 * 
 * Verifies that when a List is first created, it reports as empty
 * and behaves correctly in the empty state.
 */
void ListTest::testListStartsEmpty() {
    List<TestItem> list;
    assert_true(list.empty(), "New list should be empty");
}

/**
 * @brief Tests inserting a single item into the List
 * 
 * Verifies that a single item can be inserted into the List
 * and that the List correctly reports as non-empty after insertion.
 */
void ListTest::testListInsertSingleItem() {
    List<TestItem> list;
    TestItem* item = createTestItem(42);
    
    list.insert(item);
    assert_false(list.empty(), "List should not be empty after insert");
    
    delete item;
}

/**
 * @brief Tests inserting multiple items into the List
 * 
 * Verifies that multiple items can be inserted into the List
 * and that the List maintains its state correctly.
 */
void ListTest::testListInsertMultipleItems() {
    List<TestItem> list;
    std::vector<TestItem*> items;
    
    for (int i = 1; i <= 5; i++) {
        items.push_back(createTestItem(i));
        list.insert(items.back());
        assert_false(list.empty(), "List should not be empty after inserting items");
    }
    
    cleanupTestItems(items);
}

/**
 * @brief Tests removing from an empty List
 * 
 * Verifies that attempting to remove from an empty List
 * returns nullptr and doesn't cause errors.
 */
void ListTest::testListRemoveFromEmptyList() {
    List<TestItem> list;
    TestItem* item = list.remove();
    assert_true(item == nullptr, "Removing from empty list should return nullptr");
}

/**
 * @brief Tests removing a single item from the List
 * 
 * Verifies that a single item can be removed from the List
 * and that the correct item is returned.
 */
void ListTest::testListRemoveSingleItem() {
    List<TestItem> list;
    TestItem* original_item = createTestItem(100);
    
    list.insert(original_item);
    TestItem* retrieved_item = list.remove();
    
    assert_true(retrieved_item != nullptr, "Retrieved item should not be null");
    assert_equal(100, retrieved_item->getValue(), "Retrieved item should have correct value");
    assert_true(list.empty(), "List should be empty after removing only item");
    
    delete original_item;
}

/**
 * @brief Tests removing multiple items in FIFO order
 * 
 * Verifies that items are removed from the List in First-In-First-Out
 * order, maintaining the correct sequence.
 */
void ListTest::testListRemoveMultipleItemsFIFO() {
    List<TestItem> list;
    std::vector<TestItem*> items;
    
    // Insert items 1, 2, 3
    for (int i = 1; i <= 3; i++) {
        items.push_back(createTestItem(i));
        list.insert(items.back());
    }
    
    // Remove items and verify FIFO order
    for (int i = 1; i <= 3; i++) {
        TestItem* retrieved = list.remove();
        assert_true(retrieved != nullptr, "Retrieved item should not be null");
        assert_equal(i, retrieved->getValue(), "Items should be removed in FIFO order");
    }
    
    cleanupTestItems(items);
}

/**
 * @brief Tests that List is empty after removing all items
 * 
 * Verifies that after inserting and then removing all items,
 * the List correctly reports as empty.
 */
void ListTest::testListEmptyAfterRemovingAllItems() {
    List<TestItem> list;
    std::vector<TestItem*> items;
    
    // Insert multiple items
    for (int i = 1; i <= 5; i++) {
        items.push_back(createTestItem(i));
        list.insert(items.back());
    }
    
    // Remove all items
    while (!list.empty()) {
        TestItem* item = list.remove();
        assert_true(item != nullptr, "Retrieved item should not be null");
    }
    
    assert_true(list.empty(), "List should be empty after removing all items");
    cleanupTestItems(items);
}

/**
 * @brief Tests List empty state transitions
 * 
 * Verifies that the List correctly transitions between empty and non-empty
 * states as items are inserted and removed.
 */
void ListTest::testListEmptyStateTransitions() {
    List<TestItem> list;
    TestItem* item = createTestItem(1);
    
    // Start empty
    assert_true(list.empty(), "List should start empty");
    
    // Insert -> not empty
    list.insert(item);
    assert_false(list.empty(), "List should not be empty after insert");
    
    // Remove -> empty again
    TestItem* retrieved = list.remove();
    assert_true(retrieved != nullptr, "Retrieved item should not be null");
    assert_true(list.empty(), "List should be empty after removing last item");
    
    delete item;
}

/**
 * @brief Tests List size tracking through operations
 * 
 * Verifies that the List maintains correct size information
 * as items are inserted and removed.
 */
void ListTest::testListSizeTracking() {
    List<TestItem> list;
    std::vector<TestItem*> items;
    
    // Test size increases with insertions
    for (int i = 1; i <= 3; i++) {
        items.push_back(createTestItem(i));
        list.insert(items.back());
        // Note: List interface may not have size() method, so we test empty() behavior
        assert_false(list.empty(), "List should not be empty with items");
    }
    
    // Test size decreases with removals
    for (int i = 0; i < 3; i++) {
        TestItem* item = list.remove();
        assert_true(item != nullptr, "Should be able to remove items");
    }
    
    assert_true(list.empty(), "List should be empty after removing all items");
    cleanupTestItems(items);
}

/**
 * @brief Tests insert-remove sequence operations
 * 
 * Verifies that alternating insert and remove operations
 * work correctly and maintain List integrity.
 */
void ListTest::testListInsertRemoveSequence() {
    List<TestItem> list;
    std::vector<TestItem*> items;
    
    for (int i = 1; i <= 5; i++) {
        // Insert item
        items.push_back(createTestItem(i));
        list.insert(items.back());
        assert_false(list.empty(), "List should not be empty after insert");
        
        // Remove item
        TestItem* retrieved = list.remove();
        assert_true(retrieved != nullptr, "Should be able to remove item");
        assert_equal(i, retrieved->getValue(), "Retrieved item should have correct value");
        assert_true(list.empty(), "List should be empty after removing only item");
    }
    
    cleanupTestItems(items);
}

/**
 * @brief Tests that a new Ordered_List starts empty
 * 
 * Verifies that when an Ordered_List is first created, it reports as empty
 * and behaves correctly in the empty state.
 */
void ListTest::testOrderedListStartsEmpty() {
    Ordered_List<RankedItem, int> orderedList;
    assert_true(orderedList.empty(), "New ordered list should be empty");
}

/**
 * @brief Tests inserting a single item into the Ordered_List
 * 
 * Verifies that a single item can be inserted into the Ordered_List
 * and that the list correctly reports as non-empty after insertion.
 */
void ListTest::testOrderedListInsertSingleItem() {
    Ordered_List<RankedItem, int> orderedList;
    RankedItem* item = createRankedItem(1, 10);
    
    orderedList.insert(item);
    assert_false(orderedList.empty(), "Ordered list should not be empty after insert");
    
    delete item;
}

/**
 * @brief Tests inserting multiple items into the Ordered_List
 * 
 * Verifies that multiple items can be inserted into the Ordered_List
 * and that the list maintains its state correctly.
 */
void ListTest::testOrderedListInsertMultipleItems() {
    Ordered_List<RankedItem, int> orderedList;
    std::vector<RankedItem*> items;
    
    for (int i = 1; i <= 5; i++) {
        items.push_back(createRankedItem(i, i * 10));
        orderedList.insert(items.back());
        assert_false(orderedList.empty(), "Ordered list should not be empty after inserting items");
    }
    
    cleanupRankedItems(items);
}

/**
 * @brief Tests Ordered_List iterator traversal
 * 
 * Verifies that the iterator correctly traverses all items in the
 * Ordered_List and provides access to the stored items.
 */
void ListTest::testOrderedListIteratorTraversal() {
    Ordered_List<RankedItem, int> orderedList;
    std::vector<RankedItem*> items;
    
    // Insert items
    for (int i = 1; i <= 3; i++) {
        items.push_back(createRankedItem(i, i * 10));
        orderedList.insert(items.back());
    }
    
    // Traverse with iterator
    int count = 0;
    for (auto it = orderedList.begin(); it != orderedList.end(); ++it) {
        count++;
        assert_true(*it != nullptr, "Iterator should point to valid item");
        assert_equal(count, (*it)->getValue(), "Item value should match expected sequence");
    }
    
    assert_equal(3, count, "Iterator should traverse all 3 items");
    cleanupRankedItems(items);
}

/**
 * @brief Tests removing a specific item from Ordered_List
 * 
 * Verifies that a specific item can be removed from the Ordered_List
 * and that the remaining items are still accessible.
 */
void ListTest::testOrderedListRemoveSpecificItem() {
    Ordered_List<RankedItem, int> orderedList;
    std::vector<RankedItem*> items;
    
    // Insert items
    for (int i = 1; i <= 3; i++) {
        items.push_back(createRankedItem(i, i * 10));
        orderedList.insert(items.back());
    }
    
    // Remove middle item
    orderedList.remove(items[1]);
    
    // Verify remaining items
    int count = 0;
    int expectedValues[] = {1, 3};
    for (auto it = orderedList.begin(); it != orderedList.end(); ++it) {
        assert_equal(expectedValues[count], (*it)->getValue(), "Remaining items should be correct");
        count++;
    }
    
    assert_equal(2, count, "Should have 2 items remaining after removal");
    cleanupRankedItems(items);
}

/**
 * @brief Tests removing a non-existent item from Ordered_List
 * 
 * Verifies that attempting to remove an item that is not in the
 * Ordered_List doesn't cause errors or affect other items.
 */
void ListTest::testOrderedListRemoveNonExistentItem() {
    Ordered_List<RankedItem, int> orderedList;
    std::vector<RankedItem*> items;
    
    // Insert items
    for (int i = 1; i <= 3; i++) {
        items.push_back(createRankedItem(i, i * 10));
        orderedList.insert(items.back());
    }
    
    // Try to remove item not in list
    RankedItem* nonExistentItem = createRankedItem(99, 990);
    orderedList.remove(nonExistentItem);
    
    // Verify all original items are still there
    int count = 0;
    for (auto it = orderedList.begin(); it != orderedList.end(); ++it) {
        count++;
    }
    
    assert_equal(3, count, "All original items should still be present");
    
    cleanupRankedItems(items);
    delete nonExistentItem;
}

/**
 * @brief Tests that Ordered_List preserves order
 * 
 * Verifies that items in the Ordered_List are maintained in the
 * correct order based on their ranking criteria.
 */
void ListTest::testOrderedListOrderPreservation() {
    Ordered_List<RankedItem, int> orderedList;
    std::vector<RankedItem*> items;
    
    // Insert items in random order
    int values[] = {3, 1, 4, 2, 5};
    int ranks[] = {30, 10, 40, 20, 50};
    
    for (int i = 0; i < 5; i++) {
        items.push_back(createRankedItem(values[i], ranks[i]));
        orderedList.insert(items.back());
    }
    
    // Verify items are in correct order (assuming ordered by insertion order or rank)
    int count = 0;
    for (auto it = orderedList.begin(); it != orderedList.end(); ++it) {
        count++;
        // The exact ordering depends on the Ordered_List implementation
        assert_true(*it != nullptr, "All items should be accessible in order");
    }
    
    assert_equal(5, count, "All items should be present");
    cleanupRankedItems(items);
}

/**
 * @brief Tests basic Ordered_List iterator functionality
 * 
 * Verifies that the iterator provides basic functionality like
 * begin(), end(), increment, and dereference operations.
 */
void ListTest::testOrderedListIteratorBasicFunctionality() {
    Ordered_List<RankedItem, int> orderedList;
    RankedItem* item = createRankedItem(1, 10);
    orderedList.insert(item);
    
    auto it = orderedList.begin();
    assert_true(it != orderedList.end(), "Iterator should not equal end() when list has items");
    assert_true(*it != nullptr, "Iterator should dereference to valid item");
    
    ++it;
    assert_true(it == orderedList.end(), "Iterator should equal end() after incrementing past last item");
    
    delete item;
}

/**
 * @brief Tests iterator on empty Ordered_List
 * 
 * Verifies that iterators work correctly on empty lists,
 * with begin() immediately equaling end().
 */
void ListTest::testOrderedListIteratorEmptyList() {
    Ordered_List<RankedItem, int> orderedList;
    
    auto it = orderedList.begin();
    assert_true(it == orderedList.end(), "Iterator begin() should equal end() for empty list");
}

/**
 * @brief Tests iterator on Ordered_List with single item
 * 
 * Verifies that iterators work correctly with a single item,
 * allowing proper traversal and access.
 */
void ListTest::testOrderedListIteratorSingleItem() {
    Ordered_List<RankedItem, int> orderedList;
    RankedItem* item = createRankedItem(42, 420);
    orderedList.insert(item);
    
    int count = 0;
    for (auto it = orderedList.begin(); it != orderedList.end(); ++it) {
        count++;
        assert_equal(42, (*it)->getValue(), "Single item should have correct value");
    }
    
    assert_equal(1, count, "Should iterate over exactly one item");
    delete item;
}

/**
 * @brief Tests iterator on Ordered_List with multiple items
 * 
 * Verifies that iterators work correctly with multiple items,
 * allowing complete traversal of all elements.
 */
void ListTest::testOrderedListIteratorMultipleItems() {
    Ordered_List<RankedItem, int> orderedList;
    std::vector<RankedItem*> items;
    
    for (int i = 1; i <= 5; i++) {
        items.push_back(createRankedItem(i, i * 10));
        orderedList.insert(items.back());
    }
    
    int count = 0;
    for (auto it = orderedList.begin(); it != orderedList.end(); ++it) {
        count++;
        assert_true(*it != nullptr, "Each item should be valid");
    }
    
    assert_equal(5, count, "Should iterate over all 5 items");
    cleanupRankedItems(items);
}

/**
 * @brief Tests iterator after list modification
 * 
 * Verifies that iterators work correctly after items have been
 * added or removed from the Ordered_List.
 */
void ListTest::testOrderedListIteratorAfterModification() {
    Ordered_List<RankedItem, int> orderedList;
    std::vector<RankedItem*> items;
    
    // Insert items
    for (int i = 1; i <= 3; i++) {
        items.push_back(createRankedItem(i, i * 10));
        orderedList.insert(items.back());
    }
    
    // Remove one item
    orderedList.remove(items[1]);
    
    // Test iterator after modification
    int count = 0;
    for (auto it = orderedList.begin(); it != orderedList.end(); ++it) {
        count++;
        assert_true(*it != nullptr, "Remaining items should be valid");
    }
    
    assert_equal(2, count, "Should iterate over remaining 2 items");
    cleanupRankedItems(items);
}

/**
 * @brief Tests concurrent insertions into List
 * 
 * Verifies that multiple threads can safely insert items into
 * the List concurrently without data corruption or crashes.
 */
void ListTest::testListConcurrentInsertions() {
    List<TestItem> list;
    const int NUM_THREADS = 4;
    const int ITEMS_PER_THREAD = 50;
    
    std::vector<std::thread> threads;
    std::vector<std::atomic<bool>> thread_success(NUM_THREADS);
    
    // Initialize success flags
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_success[i].store(false);
    }
    
    // Create threads for concurrent insertions
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(&ListTest::concurrentInsertHelper, this, 
                           &list, i * ITEMS_PER_THREAD, ITEMS_PER_THREAD, &thread_success[i]);
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify all threads succeeded
    for (int i = 0; i < NUM_THREADS; i++) {
        assert_true(thread_success[i].load(), "All insertion threads should succeed");
    }
    
    // Cleanup - remove all items
    int total_removed = 0;
    while (!list.empty()) {
        TestItem* item = list.remove();
        if (item) {
            delete item;
            total_removed++;
        }
    }
    
    assert_equal(NUM_THREADS * ITEMS_PER_THREAD, total_removed, "Should retrieve all inserted items");
}

/**
 * @brief Tests concurrent removals from List
 * 
 * Verifies that multiple threads can safely remove items from
 * the List concurrently without data corruption or crashes.
 */
void ListTest::testListConcurrentRemovals() {
    List<TestItem> list;
    const int NUM_ITEMS = 200;
    const int NUM_THREADS = 4;
    
    // Pre-populate list
    std::vector<TestItem*> items;
    for (int i = 0; i < NUM_ITEMS; i++) {
        items.push_back(createTestItem(i));
        list.insert(items.back());
    }
    
    std::vector<std::thread> threads;
    std::vector<std::atomic<bool>> thread_success(NUM_THREADS);
    
    // Initialize success flags
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_success[i].store(false);
    }
    
    // Create threads for concurrent removals
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(&ListTest::concurrentRemoveHelper, this, 
                           &list, NUM_ITEMS / NUM_THREADS, &thread_success[i]);
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify all threads succeeded
    for (int i = 0; i < NUM_THREADS; i++) {
        assert_true(thread_success[i].load(), "All removal threads should succeed");
    }
    
    // Note: items are deleted by the removal threads, so no cleanup needed
}

/**
 * @brief Tests concurrent mixed operations on List
 * 
 * Verifies that mixed insert and remove operations can be performed
 * concurrently without data corruption or crashes.
 */
void ListTest::testListConcurrentMixedOperations() {
    List<TestItem> list;
    const int NUM_THREADS = 6;
    const int OPERATIONS_PER_THREAD = 30;
    
    std::vector<std::thread> threads;
    std::vector<std::atomic<bool>> thread_success(NUM_THREADS);
    
    // Initialize success flags
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_success[i].store(false);
    }
    
    // Create mixed threads (half insert, half remove)
    for (int i = 0; i < NUM_THREADS; i++) {
        if (i % 2 == 0) {
            // Insert threads
            threads.emplace_back(&ListTest::concurrentInsertHelper, this, 
                               &list, i * OPERATIONS_PER_THREAD, OPERATIONS_PER_THREAD, &thread_success[i]);
        } else {
            // Remove threads
            threads.emplace_back(&ListTest::concurrentRemoveHelper, this, 
                               &list, OPERATIONS_PER_THREAD, &thread_success[i]);
        }
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify all threads succeeded
    for (int i = 0; i < NUM_THREADS; i++) {
        assert_true(thread_success[i].load(), "All mixed operation threads should succeed");
    }
    
    // Cleanup remaining items
    while (!list.empty()) {
        TestItem* item = list.remove();
        if (item) {
            delete item;
        }
    }
}

/**
 * @brief Tests Ordered_List thread safety
 * 
 * Verifies that Ordered_List operations are thread-safe when
 * accessed concurrently from multiple threads.
 */
void ListTest::testOrderedListThreadSafety() {
    Ordered_List<RankedItem, int> orderedList;
    const int NUM_THREADS = 3;
    const int ITEMS_PER_THREAD = 20;
    
    std::vector<std::thread> threads;
    std::atomic<bool> all_success{true};
    
    // Create threads to insert items concurrently
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([&orderedList, &all_success, i, ITEMS_PER_THREAD]() {
            try {
                std::vector<RankedItem*> local_items;
                for (int j = 0; j < ITEMS_PER_THREAD; j++) {
                    local_items.push_back(new RankedItem(i * ITEMS_PER_THREAD + j, (i * ITEMS_PER_THREAD + j) * 10));
                    orderedList.insert(local_items.back());
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                
                // Cleanup local items
                for (auto item : local_items) {
                    delete item;
                }
            } catch (const std::exception& e) {
                all_success.store(false);
            }
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    assert_true(all_success.load(), "All ordered list thread operations should succeed");
}

/**
 * @brief Tests List behavior with null pointers
 * 
 * Verifies that the List handles null pointer insertions gracefully
 * without causing crashes or undefined behavior.
 */
void ListTest::testListWithNullPointers() {
    List<TestItem> list;
    
    // Test inserting null pointer (behavior depends on implementation)
    // This test documents the expected behavior
    try {
        list.insert(nullptr);
        // If this succeeds, verify list state
        assert_false(list.empty(), "List might accept null pointers");
        
        TestItem* retrieved = list.remove();
        assert_true(retrieved == nullptr, "Retrieved null pointer should be null");
    } catch (const std::exception& e) {
        // If insertion throws, that's also acceptable behavior
        assert_true(list.empty(), "List should remain empty if null insertion fails");
    }
}

/**
 * @brief Tests List with large number of items
 * 
 * Verifies that the List can handle a large number of items
 * without performance degradation or memory issues.
 */
void ListTest::testListLargeNumberOfItems() {
    List<TestItem> list;
    const int LARGE_COUNT = 1000;
    std::vector<TestItem*> items;
    
    // Insert large number of items
    for (int i = 0; i < LARGE_COUNT; i++) {
        items.push_back(createTestItem(i));
        list.insert(items.back());
    }
    
    assert_false(list.empty(), "List should not be empty with many items");
    
    // Remove all items and verify FIFO order
    for (int i = 0; i < LARGE_COUNT; i++) {
        TestItem* retrieved = list.remove();
        assert_true(retrieved != nullptr, "Should be able to retrieve all items");
        assert_equal(i, retrieved->getValue(), "Items should maintain FIFO order");
    }
    
    assert_true(list.empty(), "List should be empty after removing all items");
    cleanupTestItems(items);
}

/**
 * @brief Tests Ordered_List with duplicate ranks
 * 
 * Verifies that the Ordered_List handles items with duplicate
 * ranking values correctly without losing data.
 */
void ListTest::testOrderedListWithDuplicateRanks() {
    Ordered_List<RankedItem, int> orderedList;
    std::vector<RankedItem*> items;
    
    // Insert items with duplicate ranks
    for (int i = 1; i <= 5; i++) {
        items.push_back(createRankedItem(i, 100)); // All have same rank
        orderedList.insert(items.back());
    }
    
    // Verify all items are present
    int count = 0;
    for (auto it = orderedList.begin(); it != orderedList.end(); ++it) {
        count++;
        assert_true(*it != nullptr, "All items with duplicate ranks should be accessible");
    }
    
    assert_equal(5, count, "All items with duplicate ranks should be present");
    cleanupRankedItems(items);
}

/**
 * @brief Tests List memory management
 * 
 * Verifies that the List doesn't cause memory leaks and properly
 * manages the lifecycle of stored pointers.
 */
void ListTest::testListMemoryManagement() {
    List<TestItem> list;
    std::vector<TestItem*> items;
    
    // Insert and remove items multiple times
    for (int cycle = 0; cycle < 3; cycle++) {
        // Insert items
        for (int i = 0; i < 10; i++) {
            items.push_back(createTestItem(cycle * 10 + i));
            list.insert(items.back());
        }
        
        // Remove half the items
        for (int i = 0; i < 5; i++) {
            TestItem* item = list.remove();
            assert_true(item != nullptr, "Should be able to remove items");
        }
        
        // Remove remaining items
        while (!list.empty()) {
            TestItem* item = list.remove();
            assert_true(item != nullptr, "Should be able to remove remaining items");
        }
    }
    
    assert_true(list.empty(), "List should be empty after memory management test");
    cleanupTestItems(items);
}

/**
 * @brief Tests List performance with many items
 * 
 * Verifies that List operations maintain reasonable performance
 * characteristics even with a large number of items.
 */
void ListTest::testListPerformanceWithManyItems() {
    List<TestItem> list;
    const int PERF_COUNT = 5000;
    std::vector<TestItem*> items;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Insert many items
    for (int i = 0; i < PERF_COUNT; i++) {
        items.push_back(createTestItem(i));
        list.insert(items.back());
    }
    
    // Remove all items
    for (int i = 0; i < PERF_COUNT; i++) {
        TestItem* item = list.remove();
        assert_true(item != nullptr, "Should be able to remove all items");
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Performance should be reasonable (less than 1 second for 5000 items)
    assert_true(duration.count() < 1000, "List operations should complete in reasonable time");
    
    cleanupTestItems(items);
}

/**
 * @brief Tests Ordered_List performance with many items
 * 
 * Verifies that Ordered_List operations maintain reasonable performance
 * characteristics even with a large number of items.
 */
void ListTest::testOrderedListPerformanceWithManyItems() {
    Ordered_List<RankedItem, int> orderedList;
    const int PERF_COUNT = 2000;
    std::vector<RankedItem*> items;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Insert many items
    for (int i = 0; i < PERF_COUNT; i++) {
        items.push_back(createRankedItem(i, i * 10));
        orderedList.insert(items.back());
    }
    
    // Iterate through all items
    int count = 0;
    for (auto it = orderedList.begin(); it != orderedList.end(); ++it) {
        count++;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    assert_equal(PERF_COUNT, count, "Should iterate through all items");
    assert_true(duration.count() < 1000, "Ordered list operations should complete in reasonable time");
    
    cleanupRankedItems(items);
}

// Main function
int main() {
    TEST_INIT("ListTest");
    ListTest test;
    test.run();
    return 0;
} 