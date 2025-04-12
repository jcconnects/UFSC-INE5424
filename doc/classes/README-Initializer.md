# Initializer Framework Implementation

## Overview

This document describes the implementation of the Initializer framework, which manages the creation and lifecycle of autonomous vehicle processes and their communication components. The implementation follows the specified design patterns to create a reliable multi-process system for vehicle simulation and communication.

## Files and Their Purpose

1. **Initializer.h**:
   - Defines class interfaces and relationships
   - Integrates with actual implementations of NIC, Protocol, and Communicator
   - Establishes the friend relationships between classes
   - Defines Vehicle with responsibility for creating its own Communicator

2. **Initializer.cc**:
   - Implements the core functionality of the Initializer and Vehicle classes
   - Contains the process management logic
   - Implements the communication setup where Initializer creates NIC and Protocol

3. **test_initializer.cpp**:
   - Provides a test harness to create and manage multiple vehicles
   - Handles command-line arguments and signal handling
   - Manages the lifecycle of multiple vehicle processes
   - Demonstrates the integrated communication stack with actual implementations

## Class Relationships

The implementation consists of several related classes with a clear hierarchy and responsibility chain:

```
                   +-------------------+
                   | test_initializer  |
                   | (Main)            |
                   +-------------------+
                           |
                           | creates
                           v
                   +-------------------+
                   | Initializer       |
                   | (Process Manager) |
                   +-------------------+
                           |
                           | creates
                           v
       +------------------------------------------+
       |                   |                      |
       v                   v                      v
+-------------+    +----------------+    +-----------------+
| NIC         |<---| Protocol       |    | Vehicle         |
+-------------+    +----------------+    +-----------------+
                           ^                      |
                           |                      |
                           |                      v
                           |             +-----------------+
                           +-------------| Communicator    |
                                         +-----------------+
```

## Friend Relationship Implementation

The framework uses C++ friend relationships to manage component creation and dependency injection:

### Friend Relationship Flow

1. **Initializer is a friend of Vehicle**:
   - Allows Initializer to access Vehicle's private constructor
   - Initializer injects NIC and Protocol into Vehicle

2. **Initializer is a friend of NIC and Protocol**:
   - Enables Initializer to correctly instantiate and configure these components
   - Gives Initializer access to special configuration methods if needed

## Process Management

### Process Creation

The implementation manages vehicle processes through a parent-child relationship:

1. **InitializerTest** creates one or more **Initializer** instances
2. Each **Initializer** forks a new process using `startVehicle()`
3. Parent process tracks the child process ID
4. Child process runs the vehicle simulation using `runVehicleProcess()`

### Key Methods:

```cpp
pid_t Initializer::startVehicle() {
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process
        runVehicleProcess();
        exit(EXIT_SUCCESS);
    } else {
        // Parent process
        _vehicle_pid = pid;
        _running = true;
        return pid;
    }
}
```

## Communication Stack Setup

The framework follows a specific sequence for setting up the communication stack:

1. **Component Creation Sequence**:
   - **Initializer** creates a new **NIC<Engine>** using the actual implementation
   - **Initializer** creates a new **Protocol<NIC<Engine>>** using the actual implementation and attaches it to the NIC
   - **Initializer** creates a **Vehicle** and passes NIC and Protocol
   - **Vehicle** creates its own **Communicator** using the actual implementation

2. **Vehicle Communication**:
   - Vehicle uses its Communicator to send and receive messages
   - Communication follows the Observer pattern for asynchronous notifications
   - Actual implementations handle the message flow through the communication stack

### Key Implementation:

```cpp
template <typename Engine>
void Initializer::setupCommunicationStack() {
    // Create NIC
    auto nic = new NIC<Engine>();
    
    // Create Protocol and attach to NIC
    auto protocol = new Protocol<NIC<Engine>>(nic);
    
    // Create Vehicle with only NIC and Protocol
    Vehicle vehicle(_config, nic, protocol);
    
    // Vehicle will create its own Communicator
    vehicle.communicate();
}
```

## Component Responsibilities

Each component in the system has clearly defined responsibilities, now implemented with actual classes:

1. **Initializer**:
   - Creates and manages vehicle processes
   - Creates NIC and Protocol
   - Injects dependencies into Vehicle
   - Monitors process lifecycle

2. **Vehicle**:
   - Creates its own Communicator using the actual implementation
   - Manages the complete communication stack
   - Coordinates message sending and receiving
   - Implements the communication protocol

3. **Protocol** (Actual Implementation):
   - Formats messages for transmission
   - Handles message routing
   - Provides addressing mechanisms
   - Connects to NIC for raw communication
   - Inherits from Concurrent_Observer to receive notifications from NIC

4. **NIC** (Actual Implementation):
   - Provides network interface abstraction
   - Handles raw frames
   - Manages physical addressing
   - Uses SocketEngine for actual network I/O
   - Extends Concurrent_Observed to notify Protocol of incoming packets

5. **Communicator** (Actual Implementation):
   - Provides high-level communication API
   - Manages message queues
   - Implements asynchronous communication
   - Connects to Protocol for message transmission
   - Inherits from Concurrent_Observer to receive notifications from Protocol

## Key Design Patterns

The implementation leverages several design patterns:

1. **Modified Dependency Injection**:
   - Initializer injects only NIC and Protocol into Vehicle
   - Vehicle is responsible for creating its own Communicator
   - This better aligns with the principle that a component should manage its own dependencies

2. **Observer Pattern**:
   - NIC is observed by Protocol
   - Protocol is observed by Communicator
   - This chain of observation allows asynchronous message passing

3. **Builder Pattern**:
   - Vehicle acts as a builder for the communication stack
   - It receives the core components (NIC, Protocol) and builds the Communicator

4. **Process-based Isolation**:
   - Each vehicle runs in its own process
   - Provides better isolation and error containment

## Test Implementation

The test implementation in test_initializer.cpp demonstrates the framework's capabilities:

1. **Command-line Configuration**:
   - Number of vehicles to create
   - Communication period
   - Run duration

2. **Signal Handling**:
   - Graceful shutdown on SIGINT (Ctrl+C)
   - Proper cleanup of child processes

3. **Multiple Vehicle Creation**:
   - Creates and manages multiple vehicle processes
   - Each vehicle operates independently
   - Parent process monitors all children

## Alignment with Project Requirements

The implementation aligns with the project specifications:

1. **Process and Thread Model**:
   - Each autonomous system (vehicle) runs in its own process
   - Future components will run in threads within the vehicle process

2. **Communication Model**:
   - Uses broadcast communication
   - Leverages the Observer pattern for asynchronous notifications
   - Follows the specified communication API

3. **Dependency Structure**:
   - Follows the specified class hierarchy
   - Implements the friend relationships as required
   - Maintains proper encapsulation and information hiding

## Integration Status

This framework has been fully integrated with the actual implementations of the communication stack components:

1. **Integration with Communicator**:
   - Replaced stub implementation with actual Communicator class
   - Vehicle now creates and manages a real Communicator
   - Communicator interfaces with Protocol for message transmission

2. **Integration with Protocol and NIC**:
   - Implemented actual Protocol class that inherits from NIC::Observer (Conditional_Data_Observer)
   - Implemented actual NIC class extending Conditionally_Data_Observed for protocol-based filtering
   - Protocol contains a Concurrent_Observed member to notify Communicator components
   - Communicator inherits from Concurrent_Observer for asynchronous message handling
   - This dual observer pattern implementation follows the professor's specifications exactly
   - Complete observer chain allows for both conditional filtering and asynchronous message passing

3. **Current Limitations**:
   - SocketEngine remains a simulation layer rather than using raw Ethernet frames
   - Message passing is demonstrated but with simplified network operations
   - Network layer abstractions are in place but not fully implemented

4. **Future Extensions**:
   - Socket implementation for real network communication
   - Component threads within each vehicle process
   - Message serialization and deserialization
   - Performance metrics and latency tracking

## Usage Example

```cpp
// Create multiple initializers with configuration
for (int i = 0; i < numVehicles; i++) {
    Initializer::VehicleConfig config = {
        .id = 1000 + i,
        .period_ms = 500,
        .name = "Vehicle_" + std::to_string(i)
    };
    
    auto initializer = std::make_unique<Initializer>(config);
    initializer->startVehicle();
    initializers.push_back(std::move(initializer));
}

// Wait for all vehicles to run for specified duration
std::this_thread::sleep_for(std::chrono::seconds(runDuration));

// Shutdown all vehicles
for (auto& init : initializers) {
    init->stopVehicle();
}
```

## Conclusion

The Initializer framework provides a robust foundation for managing vehicle processes and their communication components. By following the specified design patterns and class relationships, it enables the creation and management of multiple autonomous vehicles, each with its own communication stack. The framework is designed to be extensible, supporting future enhancements while maintaining the core architecture required by the project.
