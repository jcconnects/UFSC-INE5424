# Observer Pattern Implementation

## Overview

This document describes the implementation of the Observer pattern in a thread-safe, concurrent environment. The implementation provides the foundation for asynchronous communication between components in the project, following the design patterns specified in the project requirements.

## Files and Their Purpose

1. **observer.h**:
   - Defines the Observer and Observed template classes
   - Implements the core notification mechanism
   - Provides thread-safe data structures for concurrent operation

2. **observed.h**:
   - Implements the Observed counterpart to the Observer
   - Manages registration and notification of observers
   - Contains the reference counting mechanism

3. **observer_test.cpp**:
   - Tests the Observer implementation
   - Verifies correct behavior in concurrent scenarios
   - Demonstrates proper usage patterns

## Class Relationships

The implementation consists of several related classes organized in a hierarchy:

1. `Conditional_Data_Observer<T, Condition>`: Base class for observers
2. `Conditionally_Data_Observed<T, Condition>`: Base class for observed entities
3. `Concurrent_Observer<D, C>`: Thread-safe implementation for concurrent notification
4. `Concurrent_Observed<D, C>`: Thread-safe implementation for managing observers

```
                 +---------------------------+
                 | Conditional_Data_Observer |
                 +---------------------------+
                              ^
                              |
                              | inherits
                              |
                 +---------------------------+
                 | Concurrent_Observer       |
                 +---------------------------+
                              ^
                              |
                              | observed by
                              |
+------------------+    +---------------------+
| Test Application |<-->| Concurrent_Observed |
+------------------+    +---------------------+
                              ^
                              |
                              | inherits
                              |
                 +---------------------------+
                 | Conditionally_Data_       |
                 | Observed                  |
                 +---------------------------+
```

## Observer Pattern Implementation

The Observer pattern is implemented through a hierarchy of template classes designed to handle concurrent access and notification.

### How the Observer Pattern Works Here:

1. **Observers** register with **Observed** entities for specific conditions
2. When a condition occurs, the **Observed** entity notifies all relevant **Observers**
3. Notifications are delivered asynchronously, with the Observer blocking until data arrives
4. Reference counting ensures proper memory management of shared data

## Utility Classes

### Semaphore

A POSIX semaphore wrapper that provides synchronization primitives:

```cpp
class Semaphore {
public:
    Semaphore(int count = 1);
    ~Semaphore();
    void p(); // Decrement (potentially blocking)
    void v(); // Increment (unblocks waiting threads)
private:
    sem_t _semaphore;
};
```

### Thread-Safe List

Thread-safe list implementation for storing pending notifications:

```cpp
template<typename T>
class List {
public:
    void insert(T* item);
    T* remove();
    bool empty();
private:
    std::list<T*> _list;
    std::mutex _mutex;
};
```

## Observer Base Classes

### Conditional_Data_Observer

```cpp
template<typename T, typename Condition>
class Conditional_Data_Observer {
public:
    Conditional_Data_Observer(Condition c);
    virtual ~Conditional_Data_Observer();
    
    Condition rank();
    virtual void update(Condition c, T* d) = 0;
    
private:
    Condition _rank;
};
```

### Conditionally_Data_Observed

```cpp
template<typename T, typename Condition>
class Conditionally_Data_Observed {
public:
    void attach(Conditional_Data_Observer<T, Condition>* obs, Condition c);
    void detach(Conditional_Data_Observer<T, Condition>* obs, Condition c);
    bool notify(Condition c, T* d);
    
private:
    Ordered_List<Conditional_Data_Observer<T, Condition>, Condition> _observers;
};
```

## Concurrent Implementation

### Concurrent_Observer

The `Concurrent_Observer` template provides thread-safe observation with:

1. **Responsibilities**:
   - Receive notifications from observed entities
   - Queue incoming data for processing
   - Block until data is available when requested
   - Manage reference counting for shared data

2. **Key Methods**:
   - **update()**: Receives notifications and queues data
   - **updated()**: Blocks until data is available, then returns it

### Concurrent_Observed

The `Concurrent_Observed` template provides thread-safe notification with:

1. **Responsibilities**:
   - Maintain a registry of observers by condition
   - Notify relevant observers when conditions occur
   - Manage reference counting for shared data

2. **Key Methods**:
   - **attach()**: Registers an observer for a specific condition
   - **detach()**: Unregisters an observer
   - **notify()**: Notifies all matching observers of an event

## Reference Counting Mechanism

The implementation uses reference counting for memory management of shared data objects:

1. **Data Structure**:
   ```cpp
   struct TestData {
       int value;
       std::atomic<int> ref_count;
       
       TestData(int v) : value(v), ref_count(0) {}
   };
   ```

2. **Lifecycle Management**:
   - **Creation**: Data objects are created with reference count 0
   - **Distribution**: Each notified observer increments the count
   - **Processing**: After processing, each observer decrements the count
   - **Deletion**: When count reaches zero, the last observer deletes the object

3. **Thread Safety**:
   - Uses atomic operations to ensure thread-safe counter manipulation
   - Prevents race conditions in multi-threaded environments
   - Ensures memory safety without explicit locks

## Test Implementation

The test suite demonstrates and validates the implementation through:

1. **Basic Functionality Test**:
   - Tests observer registration and notification
   - Verifies correct delivery of notifications
   - Ensures proper cleanup and shutdown

2. **Concurrent Access Test**:
   - Tests with multiple observers and conditions
   - Runs multiple producer threads sending notifications
   - Verifies thread safety and correct message delivery
   - Confirms reference counting works correctly

## Alignment with Project Requirements

The implementation aligns with the project specifications:

1. **Concurrency Support**:
   - Fully thread-safe implementation for concurrent access
   - Supports multiple observers and observed entities

2. **Observer Pattern Compliance**:
   - Follows the specified template structure
   - Implements condition-based filtering of notifications
   - Supports the required notification mechanics

3. **Thread Synchronization**:
   - Uses semaphores for thread synchronization as specified
   - Implements blocking behavior when waiting for notifications
   - Ensures thread safety for shared data access

## Implementation Status

The Observer pattern implementation is not just theoretical but has been fully realized in the project:

1. **Actual Component Integration**:
   - The Communicator class extends Concurrent_Observer to receive messages
   - The Protocol class extends Concurrent_Observer to receive raw packets
   - The NIC class extends Concurrent_Observed to notify Protocol
   - The Protocol class contains a Concurrent_Observed to notify Communicators

2. **Working Observer Chain**:
   - A complete observer chain has been implemented: NIC → Protocol → Communicator
   - Messages flow through this chain using the observer notifications
   - Reference counting ensures proper resource management throughout

3. **Practical Validation**:
   - Integration tests demonstrate the observer chain working end-to-end
   - Multiple vehicles can send and receive messages in separate processes
   - The asynchronous nature of the pattern is validated with concurrent operations

## Integration Potential

This implementation serves as the foundation for the communication system:

1. **Base for Communicator**:
   - The `Communicator` class extends `Concurrent_Observer`
   - Protocols use `Concurrent_Observed` to notify communicators
   - This pattern is now fully implemented in the actual classes

2. **Interface Consistency**:
   - Interfaces match the professor's specifications
   - Templates allow for various data types and condition types

3. **Extension Points**:
   - Can be extended to support more complex notification filtering
   - Adaptable to different types of synchronization primitives
   - Supports future extensions for prioritized notifications

## Usage Example

```cpp
// Create observed entity
TestObserved observed;

// Create and start observers
TestObserver observer1(1); // Observing condition 1
TestObserver observer2(2); // Observing condition 2

// Register observers
observed.attach(&observer1, 1);
observed.attach(&observer2, 2);

// Start observer threads
std::thread thread1(&TestObserver::run, &observer1);
std::thread thread2(&TestObserver::run, &observer2);

// Generate notifications
observed.generateData(1, 42); // Only observer1 gets this
observed.generateData(2, 84); // Only observer2 gets this

// Wait for processing
thread1.join();
thread2.join();
```

## Conclusion

The Observer pattern implementation provides a robust foundation for asynchronous, event-driven communication between components. The thread-safe, template-based design allows for flexible usage in various contexts while ensuring proper resource management through reference counting. This implementation forms the cornerstone of the communication stack, enabling the higher-level components to communicate reliably in a concurrent environment.
