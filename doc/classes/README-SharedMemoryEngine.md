# SharedMemoryEngine

## Overview

The `SharedMemoryEngine` class provides an implementation of a communication engine that uses shared memory for inter-component communication within the same process. It serves as an alternative to the `SocketEngine` class, which uses raw sockets for communication between different processes.

## Purpose

The purpose of the `SharedMemoryEngine` is to enable efficient communication between components of the same autonomous system that run in the same process, without the overhead of network communication. It maintains compatibility with the `SocketEngine` interface to allow seamless integration with the rest of the communication stack, particularly the `NIC` class.

## UML Diagram

```
+------------------+     
|       NIC        |     
+------------------+     
         |                
         | uses           
         v                
+------------------+     
| SharedMemoryEngine |    
+------------------+     
```

For a more detailed view, see the [SharedMemoryEngine UML diagram](../uml/sharedMemoryEngine.puml).

## Interface

### Constants

```cpp
static const unsigned int MAX_FRAME_SIZE = 1518;  // Max Ethernet frame size
static const unsigned int MAX_COMPONENTS = 256;   // Maximum number of components
static const unsigned int MAX_QUEUED_FRAMES = 128; // Maximum frames in queue
```

### Types

```cpp
typedef unsigned int ComponentId;  // Component identifier type

// Frame structure
struct SharedFrame {
    ComponentId src;
    ComponentId dst;
    unsigned int size;
    unsigned int protocol;
    unsigned char data[MAX_FRAME_SIZE];
};
```

### Constructors and Destructors

```cpp
SharedMemoryEngine(ComponentId id);
virtual ~SharedMemoryEngine();
```

### Public Methods

```cpp
const bool running();  // Check if engine is running
int send(Ethernet::Frame* frame, unsigned int size);  // Send a frame
void stop();  // Stop the engine
ComponentId getId() const;  // Get component ID
```

### Protected Methods

```cpp
virtual void handleSignal() = 0;  // Handle received signals, to be implemented by derived classes
```

### Private Methods

```cpp
void setupSharedMemory();  // Set up shared memory
void setupEpoll();  // Set up event polling
bool readFrame(SharedFrame& frame);  // Read a frame from the queue
static void* run(void* arg);  // Thread function for receiving messages
```

## Implementation Details

### Component Identification

Each component using the `SharedMemoryEngine` is assigned a unique `ComponentId` which is used to route messages between components. The engine also generates a MAC-like address from this ID to maintain compatibility with the Ethernet-based communication stack.

### Message Routing

Messages are routed based on the destination component ID, which is extracted from the destination MAC address of the Ethernet frame. The engine maintains a global registry of all active components and their notification file descriptors.

### Producer-Consumer Model

The `SharedMemoryEngine` implements the Producer-Consumer pattern as specified in the project requirements:

- **Producers**: Components that send messages through the `send()` method, producing frames for other components to consume
- **Consumers**: Components that process frames through the `handleSignal()` method, consuming messages from their queues
- **Shared Resource**: The component message queues (`_component_queues`), where each component has its own queue
- **Synchronization**: 
  - `_queue_mutex` protects access to the queues
  - `eventfd` provides the wake-up mechanism for consumers
  - `epoll` enables efficient waiting for notifications

The flow follows a classic Producer-Consumer pattern:
1. Producer calls `send()` to send a frame to another component
2. Producer adds the frame to the consumer's queue using `addFrameToQueue()`
3. Producer notifies the consumer by writing to its `_notify_fd`
4. Consumer is awakened from `epoll_wait()` in the `run()` method
5. Consumer calls `handleSignal()` which reads frames from its queue using `readFrame()`

This implementation ensures thread-safe, asynchronous communication between components, with proper synchronization to prevent race conditions while maintaining high performance.

### Event Polling Mechanism

The `SharedMemoryEngine` uses Linux's `epoll` facility to efficiently handle asynchronous message reception:

1. **Creating the epoll instance**: During initialization, `setupEpoll()` creates an epoll file descriptor with `epoll_create1(0)`.
   
2. **Registering notification channels**: Each engine instance registers its notification file descriptor (created with `eventfd()`) with epoll to monitor for incoming data events.

3. **Efficient waiting**: In the `run()` method, the engine calls `epoll_wait()` to efficiently block until notifications arrive, without consuming CPU resources.

The epoll mechanism provides several benefits:
- Components can wait for messages from multiple sources with minimal overhead
- No busy-waiting or constant polling is required
- The system efficiently wakes up only when actual message notifications arrive
- Scales well with increasing numbers of components

This event-based approach is essential for implementing the asynchronous event propagation required by the project, following the Observer X Observed design pattern.

### Notification Mechanism

The engine uses `eventfd` to notify components when new messages are available. When a component sends a message, it writes a value to the recipient's notification file descriptor, which triggers the `handleSignal()` method in the recipient's engine.

### Thread Safety

Thread safety is ensured through:
- Mutex protection for shared data structures
- Atomic operations for shared state
- Thread-safe notifications using `eventfd`
- Proper queue management for received frames

### Shared Memory Management

In this implementation, shared memory is simulated using in-process data structures. In a real implementation for inter-process communication, this would involve:
1. Allocating a shared memory region using `mmap` with `MAP_SHARED`
2. Creating a shared queue of frames in this region
3. Using inter-process synchronization primitives

### Comparison with SocketEngine

| Feature | SharedMemoryEngine | SocketEngine |
|---------|-------------------|-------------|
| Communication Scope | Between components in the same process | Between processes on the same or different machines |
| Performance | High (no network overhead) | Lower (network stack overhead) |
| Component Addressing | Component IDs | MAC addresses |
| Hardware Requirements | None (uses process memory) | Network interface card |
| Communication Method | Shared memory | Raw sockets |

## Usage Example

```cpp
// Create a SharedMemoryEngine with component ID 1
SharedMemoryEngine engine(1);

// Prepare an Ethernet frame
Ethernet::Frame frame;
// ... set frame properties ...

// Send the frame
engine.send(&frame, frame_size);

// When done, stop the engine
engine.stop();
```

## Integration with NIC

The `NIC` class can use either `SocketEngine` or `SharedMemoryEngine` as its underlying engine:

```cpp
// Using SocketEngine
NIC<SocketEngine> nic_socket;

// Using SharedMemoryEngine
NIC<SharedMemoryEngine> nic_shared_memory(component_id);
```

## Extension Points

The `SharedMemoryEngine` can be extended in several ways:

1. Implementing true inter-process communication using real shared memory
2. Adding quality-of-service features like priority queues
3. Implementing more sophisticated routing algorithms
4. Adding compression or encryption for messages
5. Implementing flow control mechanisms

## Implementation Challenges and Solutions

During the implementation and testing of the `SharedMemoryEngine`, several challenges were encountered and solved:

### 1. Frame Source MAC Address Consistency

**Problem**: When sending a frame, the source MAC address was being set after creating the shared frame but before copying the frame data, causing the updated MAC address not to be included in the copied data.

**Solution**: Moved the source MAC address assignment to occur before creating the shared frame and copying the data:

```cpp
// Make sure the source address is set correctly to maintain consistency
// between component ID and frame source address
frame->src = _mac_address;

// Then create and populate the shared frame
SharedFrame shared_frame;
// ... rest of the code ...
```

### 2. Memory Alignment Issues

**Problem**: Direct casting of byte arrays to structured data types caused memory alignment problems:

```cpp
// Problematic approach
Ethernet::Frame* eth_frame = reinterpret_cast<Ethernet::Frame*>(frame.data);
```

This led to incorrect data access and unpredictable behavior when accessing frame fields.

**Solution**: Used `memcpy()` to create properly aligned copies of the Ethernet frames:

```cpp
// Safe approach
Ethernet::Frame eth_frame;
memcpy(&eth_frame, frame.data, sizeof(Ethernet::Frame));
```

This ensures the data is correctly copied into an aligned structure, preventing undefined behavior caused by alignment issues.

### 3. Thread-Safety and Race Conditions

**Problem**: Multiple components accessing shared data structures concurrently could lead to race conditions.

**Solution**: Implemented proper mutex locks for all shared data structures:
- Used `_global_mutex` to protect the registry of component notification file descriptors
- Used `_queue_mutex` to protect component message queues
- Used thread-safe notifications with `eventfd`

### 4. Scope Resolution and Access Issues

**Problem**: Compilation errors related to accessing static members and protected methods.

**Solution**: Fixed scope resolution with proper use of the scope resolution operator (::) and moved certain methods from private to protected to allow access in derived test classes.

## Limitations

Current limitations of the `SharedMemoryEngine` include:

1. It simulates shared memory using process-local data structures
2. It has fixed maximum sizes for frames and component counts
3. It does not implement true zero-copy message passing
4. It does not handle component failures

## Testing

The `SharedMemoryEngine` is tested using the `sharedMemoryEngine_test.cpp` test, which verifies:

1. Component ID assignment and MAC address generation
2. Direct message transmission between components
3. Broadcast message handling
4. Thread safety and race condition avoidance
5. Proper engine shutdown 