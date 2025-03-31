# Communication Stack Implementation

## Overview

This document describes the implementation of the communication stack components that have replaced the initial stub implementations. These components include the Network Interface Card (NIC), Protocol, and SocketEngine classes, which together form a functional communication layer for the autonomous vehicle simulation framework.

## Components

### 1. SocketEngine

The `SocketEngine` class provides a simulation layer for network operations without requiring actual network hardware. This serves as the lowest level of the communication stack.

#### File: `include/stubs/socketengine.h`

#### Methods

- **Constructor** `SocketEngine()`
  - Creates a new SocketEngine instance
  - Outputs creation message to console for debugging
  
- **Destructor** `~SocketEngine()`
  - Cleans up SocketEngine resources
  - Outputs destruction message to console for debugging
  
- **send(const void* data, unsigned int size)**
  - Simulates sending data over a network socket
  - Parameters:
    - `data`: Pointer to the data buffer to send
    - `size`: Size of the data in bytes
  - Returns: Number of bytes "sent" (typically returns the full size parameter)
  - Currently just simulates successful transmission and logs the operation
  
- **receive(void* data, unsigned int size)**
  - Simulates receiving data from a network socket
  - Parameters:
    - `data`: Buffer to store received data
    - `size`: Maximum number of bytes to receive
  - Returns: Number of bytes "received"
  - Currently just simulates successful reception without actually modifying the buffer
  
- **getLocalAddress()**
  - Gets the local network address
  - Returns: A string representation of the local address ("local_address")
  - Currently returns a fixed string as a placeholder

#### Implementation Notes

- The current implementation is a simulation layer that doesn't actually communicate over a network
- It provides the required interface for the NIC to use, making it easy to replace with a real implementation later
- Detailed logging for debugging purposes helps track the flow of operations

### 2. Network Interface Card (NIC)

The `NIC` template class serves as an abstraction layer for network operations, providing a higher-level interface above the SocketEngine.

#### File: `include/nic.h`

#### Template Parameters

- **Engine**: The underlying engine type (typically SocketEngine) that handles the actual network I/O

#### Type Definitions

- **Address**: Represents a network address (currently implemented as std::string)
- **Protocol_Number**: Identifies the protocol (currently implemented as int)
- **Buffer**: Buffer type for holding data (BufferStub)
- **Observer**: Type of observers that can attach to this NIC (`Conditional_Data_Observer<Buffer, Protocol_Number>`)
- **Observed**: Type for the observation mechanism (`Conditionally_Data_Observed<Buffer, Protocol_Number>`)
- **Statistics**: Structure storing network statistics (packets/bytes sent/received)

#### Methods

- **Constructor** `NIC()`
  - Creates a new NIC instance with an internal Engine
  - Initializes the statistics counters to zero
  - Outputs creation message to console for debugging
  
- **Destructor** `~NIC()`
  - Cleans up NIC resources
  - Outputs destruction message to console for debugging
  
- **send(Address dst, Protocol_Number prot, const void\* data, unsigned int size)**
  - Sends data to a specific destination address using a specific protocol
  - Parameters:
    - `dst`: Destination address
    - `prot`: Protocol number
    - `data`: Pointer to the data buffer to send
    - `size`: Size of the data in bytes
  - Returns: Number of bytes sent
  - Updates the statistics (packets_sent, bytes_sent)
  
- **receive(Address\* src, Protocol_Number\* prot, void\* data, unsigned int size)**
  - Receives data and stores information about the sender
  - Parameters:
    - `src`: Pointer to store the source address (if not NULL)
    - `prot`: Pointer to store the protocol number (if not NULL)
    - `data`: Buffer to store received data
    - `size`: Maximum number of bytes to receive
  - Returns: Number of bytes received
  - Updates the statistics (packets_received, bytes_received)
  
- **receive(Buffer\* buf, Address\* src, Address\* dst, void\* data, unsigned int size)**
  - Processes a received buffer and extracts information
  - Parameters:
    - `buf`: Buffer containing the received data
    - `src`: Pointer to store the source address (if not NULL)
    - `dst`: Pointer to store the destination address (if not NULL)
    - `data`: Buffer to copy the data into
    - `size`: Maximum number of bytes to copy
  - Returns: Number of bytes copied from the buffer
  
- **alloc(Address dst, Protocol_Number prot, unsigned int size)**
  - Allocates a buffer for sending data
  - Parameters:
    - `dst`: Destination address
    - `prot`: Protocol number
    - `size`: Size of the buffer to allocate
  - Returns: Pointer to the allocated buffer
  
- **send(Buffer\* buf)**
  - Sends a pre-allocated buffer
  - Parameters:
    - `buf`: Buffer to send
  - Returns: Number of bytes sent
  - Updates the statistics (packets_sent, bytes_sent)
  
- **free(Buffer\* buf)**
  - Frees a buffer after use
  - Parameters:
    - `buf`: Buffer to free
  
- **address()**
  - Gets the local address
  - Returns: Reference to the local address
  
- **address(Address address)**
  - Sets the local address
  - Parameters:
    - `address`: The address to set
  
- **statistics()**
  - Gets the network statistics
  - Returns: Reference to the Statistics structure

- **attach(Observer\* obs, Protocol_Number prot)**
  - Registers an observer for a specific protocol
  - Parameters:
    - `obs`: The observer to register
    - `prot`: The protocol number to observe
  - Implementation aligns with the professor's Conditional_Data_Observed pattern

- **detach(Observer\* obs, Protocol_Number prot)**
  - Unregisters an observer
  - Parameters:
    - `obs`: The observer to unregister
    - `prot`: The protocol number that was being observed
  - Implementation aligns with the professor's Conditional_Data_Observed pattern

#### Observer Pattern Implementation

- The NIC class extends `Conditionally_Data_Observed<Buffer, Protocol_Number>`, making it an observable object
- Protocols can attach as observers to receive notifications when packets arrive for specific protocol numbers
- The notification includes the buffer received and the protocol number
- This implements the condition-based observer pattern from the professor's specifications

#### Implementation Notes

- The current implementation simulates network operations using the SocketEngine
- Statistics tracking is thread-safe using atomic counters
- Memory management for buffers is manually handled with alloc/free methods
- The Observer pattern implementation enables conditional notification of received packets

### 3. Protocol

The `Protocol` template class handles message routing and addressing, sitting between the NIC and Communicator layers.

#### File: `include/protocol.h`

#### Template Parameters

- **NICType**: The type of NIC to use

#### Type Definitions

- **Buffer**: The buffer type used by the NIC (typically BufferStub)
- **Physical_Address**: The physical address type used by the NIC
- **Port**: The port type (currently implemented as int)
- **Observer**: Type of observers that can attach to this Protocol (`Conditional_Data_Observer<Buffer, Port>`)
- **Observed**: Type for the observation mechanism (`Conditionally_Data_Observed<Buffer, Port>`)

#### Address Class

The `Address` class encapsulates addressing at the Protocol layer:

- **Constructor** `Address()`
  - Creates a null address (empty physical address and port 0)
  
- **Constructor** `Address(const Null& null)`
  - Creates a null address explicitly
  
- **Constructor** `Address(Physical_Address paddr, Port port)`
  - Creates an address with specific physical address and port
  
- **BROADCAST**: Static constant representing the broadcast address ("255.255.255.255", 0)
  
- **operator bool()**
  - Checks if the address is non-null
  - Returns: true if the address has a non-empty physical address or non-zero port
  
- **operator==(const Address& a)**
  - Compares two addresses for equality
  - Returns: true if both physical address and port match

#### Protocol Methods

- **Constructor** `Protocol(NICType* nic)`
  - Creates a new Protocol instance with a reference to the NIC
  - Attaches itself as an observer to the NIC for the specified protocol number
  - Outputs creation message to console for debugging
  
- **Destructor** `~Protocol()`
  - Detaches from the NIC
  - Cleans up Protocol resources
  - Outputs destruction message to console for debugging
  
- **send(Address from, Address to, const void\* data, unsigned int size)**
  - Sends data from one address to another
  - Parameters:
    - `from`: Source address
    - `to`: Destination address
    - `data`: Pointer to the data buffer to send
    - `size`: Size of the data in bytes
  - Returns: Number of bytes sent
  - Currently passes the message directly to the NIC layer
  
- **receive(Buffer\* buf, Address\* from, void\* data, unsigned int size)**
  - Receives data from a buffer and extracts source information
  - Parameters:
    - `buf`: Buffer containing the received data
    - `from`: Pointer to store the sender's address (if not NULL)
    - `data`: Buffer to copy the data into
    - `size`: Maximum number of bytes to copy
  - Returns: Number of bytes received
  
- **attach(Observer\* obs, Address address)**
  - Registers an observer to receive notifications for a specific address
  - Parameters:
    - `obs`: The observer to register
    - `address`: The address to observe
  
- **detach(Observer\* obs, Address address)**
  - Unregisters an observer
  - Parameters:
    - `obs`: The observer to unregister
    - `address`: The address the observer was monitoring
  
- **update(typename NICType::Observed\* obs, typename NICType::Protocol_Number prot, Buffer\* buf)**
  - Handles notifications from the NIC layer (implements the Observer pattern)
  - Parameters:
    - `obs`: The observed entity (NIC)
    - `prot`: The protocol number
    - `buf`: The received buffer
  - Notifies its own observers about the received data
  - Frees the buffer if no observers were notified

#### Observer Pattern Implementation

- The Protocol class privately inherits from `Conditional_Data_Observer<NICType::Buffer, NICType::Protocol_Number>`
  to receive notifications from the NIC based on protocol number
- It also contains a `Conditionally_Data_Observed<Buffer, Port>` member to notify Communicators based on port
- This creates a chain of conditional observation: NIC → Protocol → Communicator
- The Protocol acts as both a conditional observer (to the NIC) and a conditional observable (to Communicators)

#### Implementation Notes

- The Protocol forwards messages from the NIC layer to the appropriate Communicator based on address
- The Address class encapsulates both physical address and port information
- Conditional observation enables filtering of messages based on protocol numbers and ports
- The current implementation mainly passes messages without complex routing or filtering

### 4. Communicator (Observer Pattern Integration)

While the Communicator implementation details are covered in a separate README, it's important to note its role in the observer pattern chain:

- The Communicator class inherits from `Concurrent_Observer<Buffer, Port>`
- It attaches to the Protocol to receive notifications when messages arrive for its address
- The concurrent observer pattern enables asynchronous message handling with a semaphore mechanism
- This is the only component that should use the Concurrent observer pattern as per the professor's specifications

## Integration Chain

These components are integrated to form a complete communication stack with the correct observer pattern relationships:

```
+------------------+
| Communicator     |  Concurrent_Observer - Asynchronous message handling
+--------|---------+
         | observes (concurrent)
         v
+------------------+
| Protocol         |  Conditional_Data_Observer & Conditionally_Data_Observed - Address-based filtering
+--------|---------+
         | observes (conditional)
         v
+------------------+
| NIC              |  Conditionally_Data_Observed - Protocol-based filtering
+--------|---------+
         | uses
         v
+------------------+
| SocketEngine     |  Handles actual (simulated) socket operations
+------------------+
```

### Observer Chain Implementation

The implementation uses different variations of the Observer pattern to create a notification chain:

1. The SocketEngine simulates receiving data and passes it to the NIC
2. The NIC (as a Conditionally_Data_Observed) notifies the Protocol (a Conditional_Data_Observer) by calling its `update` method with the appropriate protocol number condition
3. The Protocol processes the data and conditionally notifies the appropriate Communicator based on the address (port) condition
4. The Communicator (as a Concurrent_Observer) receives the data asynchronously and makes it available via a semaphore mechanism

This chain allows for both conditional filtering of notifications and asynchronous communication.

## Reference Counting Mechanism

Buffer objects are tracked with reference counting to ensure proper memory management:

1. When a buffer is created, its reference count is initialized to 0
2. When passed to an observer, the count is incremented
3. When an observer is done with the buffer, the count is decremented
4. When the count reaches 0, the last user frees the buffer

This ensures that buffers are only freed when all components are done with them, preventing premature deletion and memory leaks.

## Current Limitations and Future Work

### Current Limitations

1. **SocketEngine**: Still a simulation layer rather than using real network sockets
2. **Addressing**: Uses simplistic addressing that may not map directly to real network addresses
3. **Error Handling**: Limited error checking and recovery mechanisms
4. **Protocol Implementation**: Does not implement actual network protocols (like IP, TCP, UDP)

### Future Work

1. **Real Socket Implementation**: Replace SocketEngine with a real socket implementation
2. **MAC/IP Addressing**: Implement proper network addressing schemes
3. **Protocol Stack**: Implement a proper protocol stack with encapsulation/decapsulation
4. **Error Handling**: Add comprehensive error detection and recovery
5. **Performance Optimization**: Optimize buffer handling and minimize copies
6. **Security Features**: Add authentication and encryption capabilities

## Testing

The implementation includes test cases in:
- `tests/test_initializer.cpp`: Tests the entire communication stack with multiple vehicles

These tests validate:
1. Proper creation and destruction of components
2. Message sending and receiving
3. Asynchronous communication
4. Multi-process communication between vehicles

## Conclusion

This implementation provides a functional communication stack that follows the specified design patterns, particularly the Observer pattern for asynchronous communication. While it currently simulates network operations, the architecture is designed to be easily extended with real network implementations in the future.

The separation of observer patterns (Conditional vs. Concurrent) aligns with the professor's specifications, where conditional observation is used for filtering at the Protocol and NIC levels, while concurrent observation enables asynchronous communication at the Communicator level.

The clear separation of responsibilities between components and the use of templates for flexibility make this implementation a solid foundation for further development of the communication system for autonomous vehicles. 