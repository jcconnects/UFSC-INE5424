# Component Tests Documentation

This document describes the test suite for UFSC-INE5424 components. The tests are designed to verify the functionality of individual components in isolation, interactions between components, and the entire system functioning together.

## Test Organization

Tests are organized in a hierarchical structure within the `tests/` directory:

```
tests/
├── logs/               # Log files from test execution
├── unit_tests/         # Tests for individual components in isolation
│   ├── buffer_test.cpp
│   ├── ethernet_test.cpp
│   ├── list_test.cpp
│   ├── message_test.cpp
│   ├── observer_pattern_test.cpp
│   ├── sharedMemoryEngine_test.cpp
│   ├── socketEngine_test.cpp
│   └── test_utils.h
├── integration_tests/  # Tests for interactions between components
│   ├── initializer_test.cpp
│   ├── nic_test.cpp
│   ├── protocol_test.cpp
│   ├── vehicle_test.cpp
│   └── test_utils.h
├── system_tests/       # Tests for the entire system functioning together
│   ├── demo.cpp
│   └── test_utils.h
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

## Customizing Debug Output and Logging

The system provides a flexible logging mechanism through `traits.h` and `debug.h` that allows customization of log output:

### Using traits.h to Control Debug Output

The `traits.h` file defines debug settings for different components through template specialization:

```cpp
// Basic trait for all classes
template <typename T>
struct Traits {
    static const bool debugged = true;
};

// Class-specific trait specialization
template<>
struct Traits<SpecificComponent> : public Traits<void>
{
    static const bool debugged = true;  // Enable/disable debugging for this component
};
```

To control debugging output for a specific component:
1. Create a specialized trait for your component in `traits.h`
2. Set `debugged` to `true` or `false` to enable/disable debug output
3. For fine-grained control, you can also define component-specific debug settings

### Debug Level Control

The Debug class in `debug.h` provides four levels of debug output that can be enabled/disabled in the `Traits<Debug>` specialization:

```cpp
template<>
struct Traits<Debug> : public Traits<void>
{
    static const bool error = true;    // Error messages
    static const bool warning = true;  // Warning messages
    static const bool info = true;     // Informational messages
    static const bool trace = true;    // Detailed trace messages
};
```

### Using Debug Logging in Code

To add debug output in code, use the `db()` function with appropriate template parameters:

```cpp
// For a single component
db<Component>(ERR) << "This is an error message" << std::endl;
db<Component>(WRN) << "This is a warning message" << std::endl;
db<Component>(INF) << "This is an info message" << std::endl;
db<Component>(TRC) << "This is a trace message" << std::endl;

// For multiple components
db<Component1, Component2>(INF) << "This affects both components" << std::endl;
```

The message will only be displayed if:
1. The component's `debugged` trait is set to `true`
2. The corresponding debug level is enabled in `Traits<Debug>`

### Redirecting Debug Output to Files

To redirect debug output to a file instead of the console:

```cpp
// In your initialization code:
Debug::set_log_file("your_log_file.log");

// To close the log file:
Debug::close_log_file();
```

This flexibility allows you to:
- Enable detailed debugging only for components you're testing
- Silence debug output from stable components
- Focus debug output on specific message types (errors only, for example)
- Save debug output to files for later analysis

## Unit Tests

Unit tests verify the functionality of individual components in isolation.

### 1. Buffer Test (`unit_tests/buffer_test.cpp`)

Tests the functionality of the `Buffer` template class, which provides a generic buffer for storing data.

**Test Cases:**
- Creating an empty buffer
- Setting data and verifying buffer size
- Retrieving data and checking content
- Setting data with a size larger than MAX_SIZE (should be capped)
- Clearing the buffer and verifying size
- Verifying data is zeroed after clear

### 2. Ethernet Test (`unit_tests/ethernet_test.cpp`)

Tests the functionality of the `Ethernet` class, which handles Ethernet frame operations.

**Test Cases:**
- MAC address comparison (equality and inequality)
- Null address verification (should be all zeros)
- MAC address to string conversion
- Frame structure size validation
- Creating and validating frame fields
- Setting and validating payload data

### 3. List Test (`unit_tests/list_test.cpp`)

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

### 4. Message Test (`unit_tests/message_test.cpp`)

Tests the functionality of the `Message` template class, which represents a message with a maximum size.

**Test Cases:**
- Creating an empty message
- Creating a message with data
- Content verification
- Copy constructor
- Assignment operator
- Message with size larger than MAX_SIZE (should be capped)
- Self-assignment

### 5. Observer Pattern Test (`unit_tests/observer_pattern_test.cpp`)

Tests the functionality of the Observer pattern classes, which implement the Observer design pattern in both conditional and concurrent variants.

**Test Cases:**
- **Conditional Observer Pattern:**
  - Observer registration (attach) to specific conditions
  - Observer detachment (detach) from observed entities
  - Notification filtering based on conditions
  - Multiple observers for the same condition
  - Behavior when notifying with no observers for a condition
  - Data retrieval after notification

- **Concurrent Observer Pattern:**
  - Thread-safe observer registration and notification
  - Concurrent data production and consumption
  - Multiple producer threads sending notifications
  - Multiple consumer threads receiving notifications
  - Blocking behavior when waiting for notifications
  - Reference counting for shared data
  - Observer detachment while concurrent operations are running

### 6. SocketEngine Test (`unit_tests/socketEngine_test.cpp`)

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

### 7. SharedMemoryEngine Test (`unit_tests/sharedMemoryEngine_test.cpp`)

Tests the functionality of the `SharedMemoryEngine` class, which manages communication through shared memory.

**Test Cases:**
- Memory segment creation and attachment
- Proper synchronization between processes
- Message sending and receiving through shared memory
- Error handling for invalid operations
- Memory cleanup on engine shutdown

## Integration Tests

Integration tests verify the interactions between multiple components.

### 1. NIC Test (`integration_tests/nic_test.cpp`)

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

### 2. Protocol Test (`integration_tests/protocol_test.cpp`)

Tests the functionality of the `Protocol` template class, which provides a protocol layer on top of the network interface.

**Test Cases:**
- **Protocol::Address Class:**
  - Default address construction and validation
  - Address construction with specific MAC and port
  - Address comparison (equality testing)
  - Boolean evaluation of addresses (null vs. non-null)

- **Observer Pattern:**
  - Observer attachment to specific protocol ports
  - Observer notification on message reception
  - Observer detachment and verification

- **Send and Receive:**
  - Message transmission between protocol instances
  - Source and destination address handling
  - Message integrity verification after transmission
  - Buffer management during send/receive operations

- **Large Data Handling:**
  - Sending data close to MTU size
  - Data integrity verification for large messages

- **Broadcast Address:**
  - Verification of the broadcast address constants

### 3. Initializer Test (`integration_tests/initializer_test.cpp`)

Tests the functionality of the `Initializer` class, which is responsible for creating and initializing vehicle instances with their network components.

**Test Cases:**
- **Vehicle Creation:**
  - Creating vehicles with specific IDs
  - Verifying vehicle IDs are correctly set
  - Validating that vehicles are initially not running
  - Ensuring the proper initialization of all internal components (NIC, Protocol)

- **Vehicle Identity:**
  - Ensuring different vehicles have unique IDs
  - Verifying multiple vehicles created with different IDs
  - Testing creation of a batch of vehicles with sequential IDs

- **Vehicle Lifecycle:**
  - Starting vehicles and confirming running state
  - Stopping vehicles and confirming non-running state
  - Testing multiple start/stop cycles for stability

- **MAC Address Assignment:**
  - Verifying MAC addresses are correctly derived from vehicle IDs
  - Checking MAC address format (02:00:00:00:HH:LL where HHLL is the 16-bit ID)
  - Ensuring MAC address uniqueness between vehicles
  - Validating the MAC address encoding matches the expected pattern

- **Basic Functionality:**
  - Testing send functionality of created vehicles
  - Verifying send operations complete successfully
  - Testing proper resource management and cleanup

### 4. Vehicle Test (`integration_tests/vehicle_test.cpp`)

Tests the functionality of the `Vehicle` class, which manages vehicle state, components, and communication.

**Test Cases:**
- **Creation and Basic Properties:**
  - Creating vehicles with specific IDs
  - Verifying correct ID retrieval
  - Validating initial running state

- **Lifecycle Management:**
  - Starting and stopping vehicles
  - Verifying running state changes correctly
  - Testing multiple start/stop cycles

- **Component Management:**
  - Adding components to vehicles
  - Starting components explicitly
  - Stopping components explicitly
  - Verifying component state transitions
  - Ensuring correct component ownership (components reference correct vehicle)

- **Component Lifecycle Integration:**
  - Verifying components are started when vehicle starts
  - Ensuring component lifecycle follows vehicle lifecycle
  - Testing components across multiple vehicles

- **Two-Phase Shutdown:**
  - Testing the `signal_stop()` method on components
  - Verifying components properly respond to shutdown signals
  - Ensuring correct shutdown sequence is followed

- **Communication:**
  - Testing send functionality with valid parameters
  - Verifying successful message transmission
  - Testing communication between different vehicles

- **Error Handling:**
  - Testing edge cases with empty data buffers
  - Verifying receive fails when vehicle is stopped
  - Ensuring proper error responses for invalid inputs
  - Testing safe error handling for communication failures

- **Resource Management:**
  - Verifying vehicle destructor properly cleans up components
  - Testing proper destruction order to avoid memory issues

## System Tests

System tests verify the entire system functioning together.

### 1. Demo Test (`system_tests/demo.cpp`)

Tests the complete system with multiple vehicles communicating with each other.

**Test Cases:**
- Creating multiple vehicle processes
- Inter-vehicle communication
- Automatic component creation and management
- Message sending between vehicles
- Graceful process termination