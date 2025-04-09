# Message Class Implementation

## Overview

This document describes the implementation of the `Message` template class, which provides a generic container for data transmission in the communication system. The implementation follows the project requirements for handling message data with size constraints and proper memory management.

## Files and Their Purpose

1. **message.h**:
   - Defines the Message template class
   - Implements memory management for message data
   - Provides size-constrained data storage
   - Handles data copying and access

## Class Relationships

The Message class is designed to be used as a data container within the communication system:

```
                   +-------------------+
                   | Message<MaxSize>  |
                   +-------------------+
                           |
                           | contains
                           v
                   +-------------------+
                   | void* _data[]     |
                   | unsigned int _size|
                   +-------------------+
```

## Message Class Implementation

The `Message` class is implemented as a template class that takes a maximum size parameter:

```cpp
template <unsigned int MaxSize>
class Message {
    public:
        static constexpr const unsigned int MAX_SIZE = MaxSize;
        // ... methods and data members
};
```

### Key Features:

1. **Size Management**:
   - Template parameter `MaxSize` defines the maximum message size
   - Static constant `MAX_SIZE` provides access to the size limit
   - Size is enforced during construction and copying

2. **Memory Management**:
   - Uses a fixed-size array of void pointers for data storage
   - Implements proper initialization and cleanup
   - Handles memory copying safely with size checks

3. **Data Access**:
   - Provides const access to data and size
   - Ensures data integrity through const methods
   - Maintains encapsulation of internal storage

### Constructor Implementation:

1. **Default Constructor**:
   ```cpp
   Message() : _size(0) { 
       memset(_data, 0, MAX_SIZE * sizeof(void*)); 
   }
   ```
   - Initializes empty message
   - Zeroes out the data array
   - Sets size to 0

2. **Data Constructor**:
   ```cpp
   Message(const void* data, unsigned int size) { 
       _size = (size > MAX_SIZE) ? MAX_SIZE : size;
       memcpy(_data, data, _size);
   }
   ```
   - Takes external data and size
   - Enforces size limit
   - Copies data safely

### Copy Operations:

1. **Copy Constructor**:
   ```cpp
   Message(const Message& other) : _size(other._size) {
       memcpy(_data, other._data, _size);
   }
   ```
   - Creates deep copy of message
   - Preserves size information
   - Copies data safely

2. **Assignment Operator**:
   ```cpp
   Message& operator=(const Message& other) {
       if (this != &other) {
           _size = other._size;
           memcpy(_data, other._data, _size);
       }
       return *this;
   }
   ```
   - Handles self-assignment
   - Performs deep copy
   - Maintains data integrity

### Access Methods:

1. **Data Access**:
   ```cpp
   const void* data() const { 
       return _data; 
   }
   ```
   - Returns read-only access to data
   - Maintains encapsulation
   - Preserves const correctness

2. **Size Access**:
   ```cpp
   const unsigned int size() const { 
       return _size; 
   }
   ```
   - Returns current message size
   - Provides const access
   - Enables size-aware operations

## Alignment with Project Requirements

The implementation aligns with the project specifications:

1. **Size Constraints**:
   - Enforces maximum message size through template parameter
   - Prevents buffer overflows
   - Respects Channel::MTU limitations

2. **Memory Safety**:
   - Implements proper memory management
   - Uses safe copying operations
   - Prevents memory leaks

3. **Interface Compliance**:
   - Provides required data access methods
   - Maintains const correctness
   - Supports copy operations

4. **Integration Support**:
   - Works with Channel implementations
   - Compatible with Communicator class
   - Supports protocol layer requirements

## Usage Example

```cpp
// Create an empty message
Message<1024> emptyMessage;

// Create a message with data
const char* data = "Hello, world!";
Message<1024> message(data, strlen(data) + 1);

// Access message data
const void* messageData = message.data();
unsigned int messageSize = message.size();

// Copy a message
Message<1024> copy = message;

// Assign a message
Message<1024> anotherMessage;
anotherMessage = message;
```

## Conclusion

The Message class implementation provides a robust and safe container for data transmission in the communication system. Its template-based design allows for flexible size constraints while maintaining memory safety and proper data handling. The implementation follows the project requirements closely, providing a solid foundation for message passing between system components.

The class's focus on memory safety, size constraints, and proper copying operations makes it suitable for use in the communication stack, particularly when integrated with the Channel and Communicator classes. The implementation's simplicity and reliability make it a key component in the overall system architecture.
