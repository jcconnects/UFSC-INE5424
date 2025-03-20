
# README_Observer.md

## Observer Pattern Implementation

This document explains the implementation of the Observer design pattern for the Operating Systems II course project. The implementation focuses on creating a thread-safe, semaphore-based observer pattern suitable for asynchronous communication between components.

### 1. Overview

The implementation provides a foundation for asynchronous communication through the Observer pattern, with the following key features:

- Thread-safe operations with proper locking mechanisms
- POSIX semaphore-based signaling for observer notification
- Support for conditional observation based on message types/conditions
- Reference counting for proper memory management
- Two-level observer pattern hierarchy:
  - Base level: `Conditional_Data_Observer` and `Conditionally_Data_Observed`
  - Thread-safe level: `Concurrent_Observer` and `Concurrent_Observed`

### 2. Key Components

#### 2.1 Semaphore

A wrapper around POSIX semaphores providing a simple P/V interface:

```cpp
class Semaphore {
public:
    Semaphore(int count = 0);
    ~Semaphore();
    void p();  // Wait operation
    void v();  // Signal operation
private:
    sem_t _sem;
};
```

#### 2.2 Thread-safe Collections

Two collections are implemented for thread-safe operations:

**List<T>**
```cpp
template<typename T>
class List {
public:
    void insert(T* item);
    T* remove();
    bool empty() const;
private:
    mutable std::mutex _mutex;
    std::list<T*> _items;
};
```

**Ordered_List<T, C>**
```cpp
template<typename T, typename C>
class Ordered_List {
public:
    class Iterator { ... };
    void insert(T* item);
    void remove(T* item);
    Iterator begin();
    Iterator end();
private:
    mutable std::mutex _mutex;
    std::list<T*> _items;
};
```

#### 2.3 Base Observer Pattern

The base implementation providing conditional observation:

**Conditional_Data_Observer<T, Condition>**
```cpp
template <typename T, typename Condition = void>
class Conditional_Data_Observer {
public:
    Conditional_Data_Observer(Condition rank);
    virtual void update(Condition c, T* d) = 0;
    virtual Condition rank() const;
protected:
    Condition _rank;
};
```

**Conditionally_Data_Observed<T, Condition>**
```cpp
template <typename T, typename Condition = void>
class Conditionally_Data_Observed {
public:
    void attach(Conditional_Data_Observer<T, Condition>* o, Condition c);
    void detach(Conditional_Data_Observer<T, Condition>* o, Condition c);
    bool notify(Condition c, T* d);
private:
    Observers _observers;
};
```

#### 2.4 Concurrent Observer Pattern

Thread-safe observer implementation with semaphore signaling:

**Concurrent_Observer<D, C>**
```cpp
template<typename D, typename C = void>
class Concurrent_Observer {
public:
    Concurrent_Observer(C rank);
    void update(C c, D* d);
    D* updated();  // Blocks until data is available
    C rank() const;
private:
    Semaphore _semaphore;
    List<D> _data;
    C _rank;
};
```

**Concurrent_Observed<D, C>**
```cpp
template<typename D, typename C = void>
class Concurrent_Observed {
public:
    void attach(Concurrent_Observer<D, C>* o, C c);
    void detach(Concurrent_Observer<D, C>* o, C c);
    bool notify(C c, D* d);
private:
    Observers _observers;
};
```

### 3. Test Implementation

The test code demonstrates how to use the observer pattern with two test cases:

#### 3.1 Test Data Structures

```cpp
struct TestData {
    int value;
    std::atomic<int> ref_count;
};

enum class TestCondition {
    CONDITION_1,
    CONDITION_2,
    CONDITION_3
};
```

#### 3.2 Test Observer and Observed Classes

```cpp
class TestObserver : public Concurrent_Observer<TestData, TestCondition> {
    // Receives and processes messages in a separate thread
};

class TestObserved : public Concurrent_Observed<TestData, TestCondition> {
    // Generates and broadcasts messages to observers
};
```

#### 3.3 Test Cases

1. **Basic Functionality Test**
   - Tests simple 1:1 communication between observed and observers
   - Verifies correct message delivery by condition
   - Checks proper thread synchronization

2. **Concurrent Access Test**
   - Tests multiple observers for each condition
   - Tests concurrent data generation from multiple threads
   - Validates thread safety and memory management

### 4. Known Issues

1. **Segmentation Fault**
   - Reference counting mechanism is not fully thread-safe
   - Multiple observers may try to delete the same data object
   - Concurrent access to shared resources may not be properly synchronized

2. **Inconsistent Output**
   - Observer messages sometimes overlap in output
   - Sometimes observers don't process all messages before termination

3. **Memory Management**
   - Current implementation may have memory leaks in error conditions
   - Data may not be properly cleaned up if an observer crashes

4. **Termination Issues**
   - Observers may not clean up properly on program termination
   - Semaphores may remain signaled even after program exit

### 5. Potential Improvements

1. **Reference Counting**
   - Implement a more robust thread-safe reference counting mechanism
   - Consider using `std::shared_ptr` instead of raw pointers for automatic cleanup

2. **Thread Management**
   - Add proper thread cancellation support
   - Implement a clean shutdown mechanism for observers

3. **Error Handling**
   - Add more comprehensive error handling for semaphore operations
   - Implement recovery mechanisms for failed observer operations

4. **Memory Safety**
   - Use smart pointers for automatic memory management
   - Add proper cleanup on destruction

5. **Synchronization**
   - Improve thread synchronization mechanisms
   - Consider using C++17's `std::shared_mutex` for better reader/writer locks

6. **Performance Optimization**
   - Add batching support for high-frequency messages
   - Implement thread pool for observer processing
   - Add priority support for different message types

7. **API Improvements**
   - Add timeout support for the `updated()` method
   - Provide non-blocking `tryUpdate()` method
   - Add support for filtering messages based on additional criteria

8. **Extensibility**
   - Make the observer pattern more extensible with hooks
   - Add support for multi-condition observation
   - Provide serialization support for network transmission

### 6. Integration with Project

This observer implementation serves as the foundation for the communication library in the autonomous vehicle project. To integrate it:

1. Use `NIC` class as a subclass of `Conditionally_Data_Observed`
2. Implement `Protocol` as a subclass of `Conditional_Data_Observer`
3. Make `Communicator` a subclass of `Concurrent_Observer`
4. Ensure proper memory management throughout the stack

By addressing the known issues and implementing the suggested improvements, this observer pattern implementation can provide a robust foundation for asynchronous communication in the project.
