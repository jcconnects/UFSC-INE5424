# Component Tests Documentation

This document describes the test suite for UFSC-INE5424 components. The tests are designed to verify the functionality of individual components in isolation, treating related components as stubs when necessary.

## Test Organization

All component tests are organized in the `tests/components/` directory, following a modular approach to testing:

- Each component has its own dedicated test file
- Tests use a common testing utility (`tests/components/test_utils.h`)
- Test logs are stored in the `tests/logs/` directory

## Test Environment

All tests are executed through a Makefile-based system, which:

1. Creates necessary directories for test binaries and logs
2. Compiles the test code
3. Sets up testing environment (e.g., creating dummy network interfaces)
4. Executes the tests
5. Cleans up resources afterward

Tests log detailed output to the `tests/logs/` directory, while only showing success/failure status on the console.

## Running Tests

To run all tests:
```bash
make test
```

To run a specific component test:
```bash
make run_component_<component_name>_test
```

For example:
```bash
make run_component_socketEngine_test
```

## Component Tests

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

### 6. NIC Test (`components/nic_test.cpp`)

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
  
### 7. Observer Pattern Test (`components/observer_pattern_test.cpp`)

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

## Test Architecture

The tests use a common testing utility (`test_utils.h`) that provides:

- Test initialization and logging
- Assertion macros
- Logging to both console and files

Each test follows a similar pattern:
1. Initialize the test with a name
2. Create instances of the component being tested
3. Exercise component functionality through a series of test cases
4. Assert expected behavior
5. Clean up resources

## Test Results

Test results are logged to:
- Console: summary information and success/failure status
- Log files: detailed test execution information, assertions, and results

Log files are located in the `tests/logs/` directory and named after the test (e.g., `socketEngine_test.log`).