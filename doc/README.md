# Communication System Architecture

This document provides an overview of the entire communication system architecture and serves as a navigation hub for the detailed documentation of each component.

## System Overview

The communication system implements a layered architecture for network communication between autonomous vehicles. The system follows the Observer design pattern for asynchronous message passing between components.

### Architecture Diagram

```
+------------------+    +------------------+
|     Vehicle      |    |     Vehicle      |
+------------------+    +------------------+
         |                       |
         v                       v
+------------------+    +------------------+
|   Communicator   |    |   Communicator   |
+------------------+    +------------------+
         |                       |
         v                       v
+------------------+    +------------------+
|     Protocol     |    |     Protocol     |
+------------------+    +------------------+
         |                       |
         v                       v
+------------------+    +------------------+
|       NIC        |    |       NIC        |
+------------------+    +------------------+
         |                       |
         v                       v
+------------------+    +------------------+
|   SocketEngine   |    |   SocketEngine   |
+------------------+    +------------------+
         |                       |
         v                       v
+------------------+----+------------------+
|              Network                     |
+------------------------------------------+
```

### Observer Pattern Flow

The communication stack implements two types of Observer patterns:

1. **Conditional Observer Pattern** (NIC → Protocol)
   - NIC (as `Conditionally_Data_Observed`) notifies Protocol (as `Conditional_Data_Observer`) based on protocol numbers
   - Used for filtering packets without thread synchronization

2. **Concurrent Observer Pattern** (Protocol → Communicator)
   - Protocol (via `Concurrent_Observed`) notifies Communicator (as `Concurrent_Observer`) based on port numbers
   - Enables asynchronous message handling with thread synchronization via semaphores

## Component Documentation

### Core Components

| Component | Description | Detailed Documentation |
|-----------|-------------|------------------------|
| **Initializer** | Manages process creation and lifecycle for vehicles and communication components | [README-Initializer.md](classes/README-Initializer.md) |
| **Observer** | Implements thread-safe observer patterns for asynchronous communication | [README-Observer.md](classes/README-Observer.md) |

### Communication Stack

| Layer | Component | Description | Detailed Documentation |
|-------|-----------|-------------|------------------------|
| Application | **Communicator** | High-level communication endpoint for applications | [README-Communicator.md](classes/README-Communicator.md) |
| Transport | **Protocol** | Implements port-based addressing and message routing | [README-Protocol.md](classes/README-Protocol.md) |
| Network | **NIC** | Network interface card implementation for frame handling | [README-Nic.md](classes/README-Nic.md) |
| Link | **Ethernet** | Ethernet frame handling and MAC address management | [README-Ethernet.md](classes/README-Ethernet.md) |
| Physical | **SocketEngine** | Low-level network access with raw sockets and asynchronous I/O | [README-SocketEngine.md](classes/README-SocketEngine.md) |

### Data Structures

| Component | Description | Detailed Documentation |
|-----------|-------------|------------------------|
| **Message** | Generic container for communication data | [README-Message.md](classes/README-Message.md) |
| **Buffer** | Memory management for network data | [README-Buffer.md](classes/README-Buffer.md) |

## Component Relationships

### Initializer Framework

The Initializer framework manages the creation and lifecycle of vehicle processes and communication components:

- **Initializer** creates **NIC** and **Protocol** instances
- **Initializer** creates a **Vehicle** and passes NIC and Protocol to it
- **Vehicle** creates its own **Communicator** using the Protocol
- Each vehicle runs in its own process for isolation

### Communication Flow

1. **Message Creation**:
   - Application creates a **Message** with data
   - **Communicator** prepares the message for transmission

2. **Message Sending**:
   - **Communicator** passes message to **Protocol**
   - **Protocol** adds protocol headers and passes to **NIC**
   - **NIC** formats an **Ethernet** frame and passes to **SocketEngine**
   - **SocketEngine** transmits the raw frame to the network using raw sockets

3. **Message Reception**:
   - **SocketEngine** receives a raw frame via its dedicated thread
   - **SocketEngine** notifies **NIC** through its callback mechanism
   - **NIC** notifies **Protocol** based on protocol number (Conditional Observer)
   - **Protocol** processes the frame and notifies **Communicator** based on port (Concurrent Observer)
   - **Communicator** delivers the **Message** to the application

### SocketEngine Integration

The SocketEngine provides the lowest level of network access:

- Implemented with raw sockets for direct Ethernet frame transmission/reception
- Uses epoll for efficient asynchronous I/O
- Runs a dedicated thread for receiving frames
- Provides callback mechanism that integrates with the NIC layer
- Handles low-level details like interface binding and MAC address retrieval

## Implementation Details

### Memory Management

The system uses reference counting for shared buffer management:

- Buffers are created with reference count 0
- Each component that receives a buffer increments the count
- After processing, each component decrements the count
- When count reaches zero, the last component deletes the buffer

### Thread Safety

The system ensures thread safety through:

- Atomic operations for reference counting
- Semaphores for thread synchronization in the Concurrent Observer pattern
- Mutex protection for shared data structures
- Proper encapsulation and information hiding
- Dedicated thread in SocketEngine for asynchronous reception

## Usage Examples

For detailed usage examples of each component, refer to their respective README files. The basic flow for implementing a new vehicle would be:

1. Create a Vehicle configuration
2. Initialize the communication stack using the Initializer
3. Use the Communicator to send and receive messages
4. Process messages in the application logic

## Key Design Patterns

1. **Observer Pattern**: For asynchronous message passing
2. **Template-Based Design**: For type-safe component implementation
3. **Process-Based Isolation**: Each vehicle runs in its own process
4. **Dependency Injection**: Components are passed dependencies through constructors
5. **Callback Pattern**: For asynchronous event handling in SocketEngine 

## Testing Framework

The system includes a comprehensive testing framework organized in a hierarchical structure to ensure robust verification at all levels:

### Test Organization

Tests are located in the `tests/` directory and organized into three levels:

1. **Unit Tests** (`tests/unit_tests/`)
   - Test individual components in isolation
   - Verify core functionality of each class
   - Run first in the test sequence

2. **Integration Tests** (`tests/integration_tests/`)
   - Test interactions between multiple components
   - Verify correct communication between layers
   - Run after unit tests pass

3. **System Tests** (`tests/system_tests/`)
   - Test the entire system functioning together
   - Simulate real-world usage scenarios
   - Run after integration tests pass

### Network Interface Management

The tests create a dummy network interface for simulation:

- Uses a unique interface name (`test-dummy0`) to avoid conflicts with real interfaces
- Implements safety checks to ensure real network interfaces are never modified
- Interface is automatically created before tests and removed afterward
- Tests verify that the interface is a dummy interface before attempting deletion

For more details on the testing framework, see [README_tests.md](README_tests.md) and the testing directory's [README.md](../tests/README.md). 