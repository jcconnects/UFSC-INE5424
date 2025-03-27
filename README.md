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
│   ├── test_communicator.cpp
│   ├── test_initializer.cpp
│   ├── test_observer.cpp
│── bin/                  # Compiled binaries
│── Makefile              # Build and test automation
│── README.md             # Project overview
│── READMECommunicator.md # Detailed communicator documentation
│── READMEInitializer.md  # Detailed initializer documentation
│── READMEObserver.md     # Detailed observer pattern documentation
│── READMESuperStubs.md   # Documentation for implemented stubs
```

## Getting Started

### Prerequisites

- C++14 compatible compiler
- POSIX-compatible environment
- pthread library

### Building and Running Tests

```bash
# Build all tests
make all

# Run individual tests
make run_test_observer
make run_test_communicator
make run_test_initializer

# Clean build artifacts
make clean
```

## Documentation

For detailed information about each component, please refer to the following documentation:

- **READMECommunicator.md**: Details of the Communicator class implementation
- **READMEInitializer.md**: Process management and vehicle lifecycle
- **READMEObserver.md**: Observer pattern implementation details
- **READMESuperStubs.md**: Implementation details of the components that replaced stubs

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