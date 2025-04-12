# Communication Library for Autonomous Systems

This project implements a reliable and secure communication library for autonomous systems, focusing on the communication between vehicles and their components.

## Project Description

The project follows the specifications provided by the Operating Systems II course, implementing:

1. **Communication Stack**:
   - Network Interface Card (NIC) simulation
   - Protocol layer for message routing
   - Communicator class for high-level message exchange

2. **Observer Pattern Implementation**:
   - Dual observer pattern approach as specified by the professor
   - Conditional Observer pattern between NIC and Protocol for filtering
   - Concurrent Observer pattern between Protocol and Communicator for asynchronous communication

3. **Process Management**:
   - Each vehicle runs in its own process
   - Initializer framework for managing vehicle lifecycle
   - Inter-process communication through the communication stack

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
│   ├── stubs/            # Stub implementations for testing
│── tests/                # Test implementations
│   ├── demo.cpp
│── bin/                  # Compiled binaries
│── doc/                  # Documentation
│   ├── README.md         # System architecture overview
│   ├── classes/          # Component-specific documentation
│       ├── README-Communicator.md
│       ├── README-Initializer.md
│       ├── README-Observer.md
│       ├── README-Protocol.md
│       ├── README-Nic.md
│       ├── README-Ethernet.md
│       ├── README-SocketEngine.md
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

### Building and Running Tests

```bash
# Build and run tests
make 

# Clean build artifacts
make clean
```

### Demo Test Configuration

The main demo test simulates a network of autonomous vehicles:
- Creates 30 vehicles running in separate processes
- Each vehicle has a random lifetime between 10-50 seconds
- Even-numbered vehicles act as both senders and receivers
- Odd-numbered vehicles act only as receivers
- All log output is saved to the logs/ directory

## System Architecture

The complete system architecture and detailed component interactions are documented in [doc/README.md](doc/README.md). This document provides:

- A layered architecture diagram of the communication system
- Explanation of the Observer pattern implementations
- Detailed documentation for each component
- Information about communication flow and message handling
- Memory management and thread safety details

## Documentation

For detailed information about each component, please refer to the following documentation:

- **doc/README.md**: Overview of the entire communication system architecture
- **doc/classes/README-Communicator.md**: Details of the Communicator class implementation
- **doc/classes/README-Initializer.md**: Process management and vehicle lifecycle
- **doc/classes/README-Observer.md**: Observer pattern implementation details
- **doc/classes/README-Protocol.md**: Communication protocol implementation
- **doc/classes/README-Nic.md**: Network interface card implementation
- **doc/classes/README-Ethernet.md**: Ethernet frame handling
- **doc/classes/README-SocketEngine.md**: Low-level network access with raw sockets
- **doc/classes/README-Message.md**: Message container implementation
- **doc/classes/README-Buffer.md**: Memory management for network data

## Implementation Notes

This project implements a dual observer pattern approach:

1. **Conditional Observer Pattern**:
   - Used between NIC and Protocol
   - Based on protocol numbers for filtering
   - No thread synchronization involved

2. **Concurrent Observer Pattern**:
   - Used between Protocol and Communicator
   - Provides asynchronous message handling
   - Includes thread synchronization with semaphores

The implementation follows the professor's specifications exactly, providing a robust foundation for communication between autonomous systems.