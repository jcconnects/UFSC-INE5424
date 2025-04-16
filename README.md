# Communication Library for Autonomous Systems

This project implements a reliable and secure communication library for autonomous systems, focusing on the communication between vehicles and their components.

## Project Description

The project follows the specifications provided by the Operating Systems II course, implementing:

1. **Communication Stack**:
   - Network Interface Card (NIC) simulation
   - Protocol layer for message routing
   - Communicator class for high-level message exchange
   - SharedMemoryEngine for intra-vehicle communication
   - SocketEngine for inter-vehicle communication via raw sockets

2. **Observer Pattern Implementation**:
   - Dual observer pattern approach as specified by the professor
   - Conditional Observer pattern between NIC and Protocol for filtering
   - Concurrent Observer pattern between Protocol and Communicator for asynchronous communication

3. **Process Management**:
   - Each vehicle runs in its own process
   - Initializer framework for managing vehicle lifecycle
   - Inter-process communication through the communication stack
   - Components of a vehicle run as threads within the vehicle process

## Project Structure

```
project-root/
│── include/              # Header files
│   ├── communicator.h    # High-level communication API
│   ├── initializer.h     # Process management
│   ├── nic.h             # Network interface implementation
│   ├── observed.h        # Observer pattern implementation
│   ├── observer.h        # Observer pattern implementation
│   ├── protocol.h        # Communication protocol
│   ├── vehicle.h         # Vehicle implementation
│   ├── sharedMemoryEngine.h # Shared memory communication
│   ├── socketEngine.h    # Raw socket communication
│   ├── stubs/            # Stub implementations for testing
│── tests/                # Test implementations
│   ├── unit_tests/       # Tests for individual components in isolation
│   ├── integration_tests/ # Tests for interactions between components
│   ├── system_tests/     # Tests for the entire system
│   ├── demo.cpp
│   ├── test_utils.h      # Common utilities for all tests
│   ├── logs/             # Log files from test execution
│── bin/                  # Compiled binaries
│── doc/                  # Documentation
│   ├── README.md         # System architecture overview
│   ├── README-Tests.md   # Testing framework documentation
│   ├── RobustShutdownSequence/ # Shutdown protocol documentation
│   ├── uml/              # UML diagrams
│   ├── classes/          # Component-specific documentation
│       ├── README-Communicator.md
│       ├── README-Initializer.md
│       ├── README-Observer.md
│       ├── README-Protocol.md
│       ├── README-Nic.md
│       ├── README-Ethernet.md
│       ├── README-SocketEngine.md
│       ├── README-SharedMemoryEngine.md
│       ├── README-Message.md
│       ├── README-Buffer.md
│── Makefile              # Build and test automation
│── README.md             # Project overview
```

## Getting Started

### Prerequisites

- C++14 compatible compiler
- POSIX-compatible environment
- pthread library
- Raw socket capabilities (may require root privileges)

### Building and Running Tests

```bash
# Build and run tests
make 

# Clean build artifacts
make clean
```

## Documentation

For detailed information about each component, please refer to the following documentation:

- [**System Architecture**](doc/README.md): Overview of the entire communication system architecture
- [**Testing Framework**](doc/README-Tests.md): Detailed information about the testing framework
- [**Robust Shutdown Sequence**](doc/RobustShutdownSequence/README-RobustShutdownSequence.md): Documentation of the shutdown protocol
- [**Communicator**](doc/classes/README-Communicator.md): Details of the Communicator class implementation
- [**Initializer**](doc/classes/README-Initializer.md): Process management and vehicle lifecycle
- [**Observer Pattern**](doc/classes/README-Observer.md): Observer pattern implementation details
- [**Protocol**](doc/classes/README-Protocol.md): Communication protocol implementation
- [**Network Interface Card**](doc/classes/README-Nic.md): Network interface card implementation
- [**Ethernet**](doc/classes/README-Ethernet.md): Ethernet frame handling
- [**Socket Engine**](doc/classes/README-SocketEngine.md): Low-level network access with raw sockets
- [**Shared Memory Engine**](doc/classes/README-SharedMemoryEngine.md): Shared memory communication implementation
- [**Message**](doc/classes/README-Message.md): Message container implementation
- [**Buffer**](doc/classes/README-Buffer.md): Memory management for network data
