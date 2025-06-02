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

4. **Vehicle Component Architecture**:
   - Component base class for vehicle subsystems
   - Specialized components for various vehicle functions
   - Thread-safe communication between components

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
│   ├── component.h       # Base class for vehicle components
│   ├── address.h         # Address handling
│   ├── ethernet.h        # Ethernet frame handling
│   ├── socketEngine.h    # Raw socket communication
│   ├── sharedMemoryEngine.h # Shared memory communication
│   ├── message.h         # Message container
│   ├── buffer.h          # Memory management
│   ├── components/       # Specialized vehicle components
│       ├── battery_component.h
│       ├── camera_component.h
│       ├── ecu_component.h
│       ├── ins_component.h
│       ├── lidar_component.h
│── tests/                # Test implementations
│   ├── unit_tests/       # Unit tests for individual components
│   ├── integration_tests/ # Tests for component interactions
│   ├── system_tests/     # Full system tests
│── bin/                  # Compiled binaries
│── build/                # Build artifacts
│── doc/                  # Documentation
│   ├── README.md         # System architecture overview
│   ├── README_tests.md   # Testing framework documentation
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
│── logs/                 # Log output directory
│── statistics/           # Performance metrics and statistics
│── Makefile              # Build and test automation
│── Dockerfile            # Container definition for testing
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
- Creates multiple vehicles running in separate processes
- Each vehicle has various components (LiDAR, camera, ECU, etc.)
- Components communicate with each other within and across vehicles
- All log output is saved to the logs/ directory

## System Architecture

The complete system architecture and detailed component interactions are documented in [doc/README.md](doc/README.md). This document provides:

- A layered architecture diagram of the communication system
- Explanation of the Observer pattern implementations
- Detailed documentation for each component
- Information about communication flow and message handling
- Memory management and thread safety details

## Component Architecture

The component architecture consists of:

1. **Base Component Class**:
   - Thread-safe execution in dedicated threads
   - Standardized communication interface
   - Built-in logging and error handling

2. **Specialized Components**:
   - **LiDAR Component**: Generates and transmits point cloud data
   - **Camera Component**: Simulates video frame capture and transmission
   - **ECU Component**: Electronic Control Unit for vehicle management
   - **INS Component**: Inertial Navigation System for position tracking
   - **Battery Component**: Battery status monitoring and reporting

3. **Component Communication**:
   - Direct component-to-component messaging
   - Support for broadcast communications
   - Addressing based on vehicle ID and component port

## Documentation

For detailed information about each component, please refer to the following documentation:

- **doc/README.md**: Overview of the entire communication system architecture
- **doc/README_tests.md**: Details about the testing framework
- **doc/classes/README-Communicator.md**: Details of the Communicator class implementation
- **doc/classes/README-Initializer.md**: Process management and vehicle lifecycle
- **doc/classes/README-Observer.md**: Observer pattern implementation details
- **doc/classes/README-Protocol.md**: Communication protocol implementation
- **doc/classes/README-Nic.md**: Network interface card implementation
- **doc/classes/README-Ethernet.md**: Ethernet frame handling
- **doc/classes/README-SocketEngine.md**: Low-level network access with raw sockets
- **doc/classes/README-SharedMemoryEngine.md**: Shared memory communication engine
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

The implementation follows the professor's specifications, providing a robust foundation for communication between autonomous systems.

## Address Rule of Formation

Each Communicator in the system is assigned a unique address that follows a hierarchical addressing scheme:

1. **Physical Address Component (MAC Address)**:
   - Each Communicator inherits the physical MAC address of the vehicle's NIC
   - MAC addresses are 6-byte (48-bit) identifiers defined in `Ethernet::Address`

2. **Logical Address Component (Port Number)**:
   - Each Communicator within a vehicle is assigned a unique port number
   - Port 0 is reserved for the vehicle itself
   - Components are assigned sequential ports (1, 2, 3, etc.)

3. **Vehicle MAC Address Formation**:
   - Vehicle MAC addresses follow a specific format: `02:00:00:00:XX:YY`
   - The first byte is always `0x02` to indicate a locally administered address
   - The last two bytes `XX:YY` represent the 16-bit vehicle ID
   - This design limits the system to a maximum of 65,536 (2^16) unique vehicles

4. **String Representation**:
   - Addresses are formatted as `MAC:PORT` 
   - Example: `02:00:00:00:00:01:1` represents the component with port 1 on a vehicle with MAC address 02:00:00:00:00:01

This dual-layer addressing system ensures unique identification of each Communicator across the entire network.

## Multicast Capability

The hierarchical addressing scheme supports potential implementation of multicast communication:

1. **Component Groups**:
   - Components can be logically grouped by assigning similar port patterns
   - For example, all sensor components could use ports 1xx, actuators 2xx, etc.

2. **Vehicle Groups**:
   - Vehicles can be organized into functional groups using specific bits in their ID
   - Example: First 8 bits could represent the group, last 8 bits the vehicle within the group

3. **Multicast Implementation**:
   - Protocol layer can be extended to recognize special address patterns for multicast
   - Messages could be sent to all components of a certain type across multiple vehicles
   - A multicast address could use reserved patterns (e.g., specific port ranges or MAC address bits)

This addressing structure provides the flexibility to implement efficient one-to-many communication patterns in future extensions.

## Illustration of the Encapsulation

![Encapsulation](./doc/img/encapsulation.png)

## Clock Synchronization

The system implements a high-precision PTP (Precision Time Protocol) clock synchronization mechanism designed for autonomous vehicle networks. The clock synchronization parameters have been carefully chosen based on precision requirements and hardware capabilities.

### Design Rationale

#### **Precision Choice: Millisecond Resolution**
The system uses millisecond precision (`std::chrono::milliseconds`) for all timestamp operations:

- **Message transmission time**: 2us per message
- **Hardware capability**: macOS M1 Pro reliably delivers millisecond precision
- **PTP requirements**: Millisecond precision suitable for many vehicular applications
- **Efficiency**: Good balance between precision and computational overhead

#### **Timeout Configuration: 20ms Leader Silence Interval**
The `MAX_LEADER_SILENCE_INTERVAL` is set to 20 milliseconds based on cumulative error analysis:

```cpp
// Allow up to 10ms cumulative error:
// For standard crystal specification worst-case (~20 ppb): 10ms / 20ppb = 500ms
static constexpr DurationType MAX_LEADER_SILENCE_INTERVAL = std::chrono::milliseconds(500);
```

**Calculation methodology**:
- **Cumulative error limit**: 10ms (10× the 1ms precision for safety margin)
- **Assumed oscillator drift**: 20 parts per billion (ppb) - standard crystal specification worst-case
- **Formula**: `MAX_SILENCE = ERROR_LIMIT / DRIFT_RATE = 10ms / 20ppb = 500ms`

#### **Hardware Requirements**
This configuration is based on standard crystal specifications:
- **Standard Crystal Specification**: Maximum allowed long-term frequency drift of 20 ppb
- **Conservative approach**: Uses worst-case specification limits rather than typical performance
- **Real-world robustness**: Accounts for temperature variations, crystal aging, and manufacturing tolerances
- **Widely applicable**: Works with standard PC hardware and embedded systems

#### **Benefits of This Approach**

1. **Specification-based**: Uses actual hardware specifications from standard crystal documentation
2. **Conservative design**: Handles worst-case hardware performance scenarios
3. **Fast failure detection**: 500ms timeout enables rapid leader failover
4. **Robust operation**: Works reliably even with poor-quality or aged crystals

#### **Performance Characteristics**
- **Maximum drift error**: 10ms over 500ms silence period
- **Network tolerance**: Handles typical network jitter (usually < 10ms)
- **Leader failover time**: Maximum 500ms detection of failed leaders
- **Message capacity**: ~250,000 messages possible during timeout period (500ms ÷ 2μs)

This design ensures reliable clock synchronization while maintaining the precision required for safety-critical autonomous vehicle operations.

## Docker to use most recent gcc version

```bash
sudo docker build -t newestGCCenv .
sudo docker run -it --rm --privileged -v "$(pwd)":/app newestGCCenv
```

## 📖 Documentation

### Online Documentation (GitHub Pages)
The complete API documentation is automatically generated and hosted at:
**[📚 https://joaopedroschmidtcordeiro.github.io/UFSC-INE5424/](https://joaopedroschmidtcordeiro.github.io/UFSC-INE5424/)**

The documentation includes:
- Complete API reference for all classes and functions
- Class inheritance diagrams  
- File dependency graphs
- Cross-referenced source code
- Integration with the project README as the main page

*Note: Documentation is automatically updated whenever code is pushed to the main branch.*

**📋 Want to set up GitHub Pages for your own project?** See our comprehensive setup guide: [`docs-setup.md`](docs-setup.md)

### Local Documentation Generation
You can also generate the documentation locally using Doxygen:

```bash
# Generate HTML documentation
make docs

# Generate and open documentation in browser
make docs-open

# Clean generated documentation
make clean-docs
```

**Prerequisites for local documentation generation:**
- Doxygen (install with `brew install doxygen` on macOS)