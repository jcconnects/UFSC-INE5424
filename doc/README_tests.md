# Component Tests Documentation

This document describes the test suite for UFSC-INE5424 components. The tests are designed to verify the functionality of individual components in isolation, treating related components as stubs when necessary.

## Test Organization

Tests are organized in a hierarchical structure within the `tests/` directory:

```
tests/
├── logs/               # Log files from test execution
├── unit_tests/         # Tests for individual components in isolation
├── integration_tests/  # Tests for interactions between components
├── system_tests/       # Tests for the entire system functioning together
├── test_utils.h        # Common utilities for all tests
└── CMakeLists.txt      # Build configuration for tests
```

This hierarchical approach ensures proper verification at all levels:
1. **Unit level** - Testing individual components in isolation
2. **Integration level** - Testing interactions between components
3. **System level** - Testing the complete system functioning together

## Test Environment

All tests are executed through a Makefile-based system, which:

1. Creates necessary directories for test binaries and logs
2. Compiles the test code
3. Sets up testing environment (e.g., creating dummy network interfaces)
4. Executes the tests in order: unit → integration → system
5. Cleans up resources afterward

## Network Interface Configuration

The tests use a dummy network interface for network communication:

- Uses a unique interface name (`test-dummy0`) to avoid conflicts
- Implements safety checks to avoid modifying real network interfaces:
  - Verifies if an interface is a dummy interface before deletion
  - Uses a fallback interface name if the primary name exists but isn't a dummy
  - Tracks which interface was created to ensure only that one is removed
- Interface name is stored in `tests/logs/current_test_iface` and read by tests
- All tests dynamically use the interface name from this file for consistent testing

## Running Tests

To run all tests in order (unit → integration → system):
```bash
make
```

To run a specific test level:
```bash
make unit_tests
make integration_tests
make system_tests
```

To run a specific test:
```bash
make run_unit_<test_name>
make run_integration_<test_name>
make run_system_<test_name>
```

For example:
```bash
make run_unit_socketEngine
make run_integration_vehicle
make run_system_demo
```

## Test Output

Tests produce different types of output:

- **Unit tests** and **integration tests** display output directly to the console
- **System tests** redirect output to log files to keep the console clean
  - Log files are stored in `tests/logs/<test_name>.log`
  - Only success/failure status is shown on the console
  - If a test fails, the log is displayed on the console

## Unit Tests

Unit tests verify the functionality of individual components in isolation.

### 1. Buffer Test (`components/buffer_test.cpp`)

Tests the functionality of the `Buffer` template class, which provides a generic buffer for storing data.

**Test Cases:**
- Creating an empty buffer
- Setting data and verifying buffer size
- Retrieving data and checking content
- Setting data with a size larger than MAX_SIZE (should be capped)
- Clearing the buffer and verifying size
- Verifying data is zeroed after clear

### 2. Ethernet Test (`components/ethernet_test.cpp`)

Tests the functionality of the `Ethernet` class, which handles Ethernet frame operations.

**Test Cases:**
- MAC address comparison (equality and inequality)
- Null address verification (should be all zeros)
- MAC address to string conversion
- Frame structure size validation
- Creating and validating frame fields
- Setting and validating payload data

### 3. List Test (`components/list_test.cpp`)

Tests the functionality of the `List` and `Ordered_List` classes for managing collections of objects.

**Test Cases:**
- **List:**
  - Empty list verification
  - Item insertion and retrieval
  - FIFO order removal
  - Emptiness after removing all items
  - Behavior when removing from an empty list

- **Ordered_List:**
  - Empty list verification
  - Item insertion and ordering
  - Iteration through ordered items
  - Item removal and list integrity

- **Thread Safety:**
  - Concurrent insertions from multiple threads
  - Integrity of list after concurrent operations
  - Retrieval of all inserted items

### 4. Message Test (`components/message_test.cpp`)

Tests the functionality of the `Message` template class, which represents a message with a maximum size.

**Test Cases:**
- Creating an empty message
- Creating a message with data
- Content verification
- Copy constructor
- Assignment operator
- Message with size larger than MAX_SIZE (should be capped)
- Self-assignment

### 5. SocketEngine Test (`components/socketEngine_test.cpp`)

Tests the functionality of the `SocketEngine` class, which manages raw socket communication.

**Test Cases:**
- **Initialization:**
  - Socket file descriptor creation
  - Epoll file descriptor creation 
  - Interface index retrieval
  - MAC address retrieval

- **Communication:**
  - Engine running status
  - Broadcasting frames
  - Direct frame sending between two engine instances
  - Frame reception verification
  - Signal handling

- **Shutdown:**
  - Engine stopping
  - Running status after stop

### 6. SharedMemoryEngine Test (`components/sharedMemoryEngine_test.cpp`)

Tests the functionality of the `SharedMemoryEngine` class, which provides local inter-process communication.

**Test Cases:**
- **Initialization:**
  - Shared memory segment creation
  - Region mapping
  - Queue initialization

- **Communication:**
  - Message enqueueing and dequeueing
  - Message ordering
  - Thread safety during concurrent operations
  - Performance under load

- **Shutdown:**
  - Resource cleanup
  - Proper unmapping of shared memory regions

### 7. NIC Test (`components/nic_test.cpp`)

Tests the functionality of the `NIC` template class, which provides a network interface controller implementation.

**Test Cases:**
- **Address Functions:**
  - Default address verification
  - Setting a custom MAC address
  - Address retrieval and validation

- **Buffer Management:**
  - Buffer allocation with destination address and protocol
  - Frame field validation (source, destination, protocol)
  - Buffer size verification
  - Buffer freeing and reuse

- **Statistics Tracking:**
  - Initial statistics verification
  - Error condition handling
  - Drop counter increment on failed operations

### 8. Protocol Test (`components/protocol_test.cpp`)

Tests the functionality of the `Protocol` template class, which provides a protocol layer on top of the network interface.

**Test Cases:**
- **Protocol::Address Class:**
  - Default address construction and validation
  - Address construction with specific MAC and port
  - Address comparison (equality testing)
  - Boolean evaluation of addresses (null vs. non-null)

### 9. Component Test (`components/component_test.cpp`)

Tests the functionality of the `Component` base class and its derived components.

**Test Cases:**
- **Component Creation:**
  - Basic component creation with vehicle and protocol
  - Name assignment and retrieval
  - Address assignment and verification

- **Thread Management:**
  - Thread creation during start()
  - Thread termination during stop()
  - Running status tracking

- **Communication:**
  - Message sending between components
  - Message receiving with timeout
  - Handling of oversized messages

- **Lifecycle:**
  - Proper resource acquisition during initialization
  - Proper resource cleanup during termination
  - Logging file creation and closure

## Integration Tests

Integration tests verify the interactions between multiple components.

### 1. Vehicle Integration Test (`integration/vehicle_test.cpp`)

Tests the integration of Vehicle with components, NIC, and Protocol.

**Test Cases:**
- **Vehicle Creation:**
  - Initializing a vehicle with NIC and Protocol
  - Base address verification
  - Component addition and verification

- **Component Integration:**
  - Adding multiple components to a vehicle
  - Component address assignment
  - Component lifecycle managed by vehicle

- **Communication Flow:**
  - End-to-end message passing between components
  - Communication between vehicles
  - Broadcast messaging

### 2. Components Integration Test (`integration/components_test.cpp`)

Tests the interactions between different specialized components.

**Test Cases:**
- **Component Communication:**
  - LiDAR component sending data to ECU
  - Camera component sending frames to ECU
  - Battery reporting to multiple components
  - INS providing location data to other components

- **Data Processing:**
  - ECU receiving and processing sensor data
  - Components responding to commands
  - Error handling in component interactions

## System Tests

System tests verify the entire system functioning as a whole, with multiple vehicles and components operating concurrently.

### 1. Vehicle Network Test (`system/vehicle_network_test.cpp`)

Tests a network of vehicles communicating with each other.

**Test Cases:**
- **Multi-Vehicle Network:**
  - Creating multiple vehicles running concurrently
  - Inter-vehicle communication
  - Network congestion handling
  - Message routing between vehicles

### 2. Component Network Test (`system/component_network_test.cpp`)

Tests a network of components across multiple vehicles.

**Test Cases:**
- **Cross-Vehicle Component Communication:**
  - LiDAR on one vehicle sending to ECU on another
  - Coordinated behavior between components on different vehicles
  - Broadcast messaging across the component network

### 3. Full Demo Test (`system/demo_test.cpp`)

Full demonstration of the system capabilities with realistic vehicle configurations.

**Test Cases:**
- **Complete System:**
  - Multiple vehicles with full component suites
  - Dynamic vehicle creation and destruction
  - Realistic message patterns between components
  - System stability under load
  - Performance metrics collection

## Performance Tests

In addition to functional tests, the system includes performance tests to evaluate throughput, latency, and resource utilization.

- **Throughput Tests:** Measure message throughput under various loads
- **Latency Tests:** Measure end-to-end message delivery time
- **Resource Tests:** Monitor CPU, memory, and network utilization
- **Scalability Tests:** Evaluate system performance as the number of vehicles increases

Results from performance tests are stored in the `statistics/` directory for analysis and comparison between system versions.