# Communication System Architecture

This document provides an overview of the entire communication system architecture and serves as a navigation hub for the detailed documentation of each component.

## System Overview

The communication system implements a layered architecture for network communication between autonomous vehicles. The system follows the Observer design pattern for asynchronous message passing between components.

### Observer Pattern Flow

The communication stack implements two types of Observer patterns:

1. **Conditional Observer Pattern** (NIC → Protocol)
   - NIC (as `Conditionally_Data_Observed`) notifies Protocol (as `Conditional_Data_Observer`) based on protocol numbers
   - Used for filtering packets without thread synchronization

2. **Concurrent Observer Pattern** (Protocol → Communicator)
   - Protocol (via `Concurrent_Observed`) notifies Communicator (as `Concurrent_Observer`) based on port numbers
   - Enables asynchronous message handling with thread synchronization via semaphores

## Component Relationships

### Initializer Framework

The Initializer framework manages the creation and lifecycle of vehicle processes and communication components:

- **Initializer** creates **NIC** and **Protocol** instances
- **Initializer** creates a **Vehicle** and passes NIC and Protocol to it
- **Vehicle** creates its components and manages their lifecycle
- Each vehicle runs in its own process for isolation

### Component Architecture

The Component architecture follows these principles:

1. **Thread-Safe Execution**:
   - Each component runs in its own thread
   - Components start/stop in coordinated fashion through Vehicle
   - Built-in error handling and recovery mechanisms

2. **Communication Model**:
   - Components communicate through Communicator interface
   - Messages can be directed to specific components or broadcast
   - Both intra-vehicle and inter-vehicle communication supported

3. **Component Hierarchy**:
   - All components derive from the Component base class
   - Specialized components implement specific vehicle functions
   - Components have a standard interface but custom behaviors

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
- Dedicated threads for asynchronous reception

## Usage Examples

For detailed usage examples of each component, refer to their respective README files. The basic flow for implementing a new vehicle would be:

1. Create a Vehicle configuration
2. Initialize the communication stack using the Initializer
3. Add specialized components to the vehicle
4. Start the vehicle and its components
5. Process messages in component logic

## Key Design Patterns

1. **Observer Pattern**: For asynchronous message passing
2. **Template-Based Design**: For type-safe component implementation
3. **Process-Based Isolation**: Each vehicle runs in its own process
4. **Dependency Injection**: Components are passed dependencies through constructors
5. **Callback Pattern**: For asynchronous event handling in communication engines
6. **Thread-Per-Component**: Each component runs in its own thread for isolation

For more details on the testing framework, the testing directory's [README.md](../tests/README.md). 