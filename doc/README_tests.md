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

### 7. Protocol Test (`components/protocol_test.cpp`)

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

### 8. Initializer Test (`components/initializer_test.cpp`)

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

### 9. Vehicle Test (`components/vehicle_test.cpp`)

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

### 10. Observer Pattern Test (`components/observer_pattern_test.cpp`)

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