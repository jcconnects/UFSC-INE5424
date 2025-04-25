# Test Directory Structure

This directory contains all tests for the UFSC-INE5424 communication system. The tests are organized into a hierarchical structure to provide comprehensive coverage from unit-level to system-level testing.

## Directory Organization

```
tests/
├── logs/               # Log files from test execution
├── unit_tests/         # Tests for individual components in isolation
├── integration_tests/  # Tests for interactions between components
├── system_tests/       # Tests for the entire system functioning together
├── test_utils.h        # Common utilities for all tests
└── CMakeLists.txt      # Build configuration for tests
```

## Test Hierarchy

The test suite follows a layered approach:

1. **Unit Tests** (`unit_tests/`): 
   - Test individual components in isolation
   - Verify core functionality of each class
   - Examples: buffer_test, socketEngine_test, ethernet_test

2. **Integration Tests** (`integration_tests/`):
   - Test interactions between multiple components
   - Verify correct communication between layers
   - Examples: nic_test, protocol_test, vehicle_test

3. **System Tests** (`system_tests/`):
   - Test the entire system functioning together
   - Simulate real-world usage scenarios
   - Examples: demo.cpp (creates multiple vehicles communicating)

## Test Execution

Tests are executed in a specific order designed to catch issues at the lowest level first:

1. Unit tests run first
2. Integration tests run second
3. System tests run last

This approach ensures that higher-level tests only run if the lower-level components function correctly.

## Network Interface Management

The tests use a dummy network interface for simulation:

- A unique interface name (`test-dummy0`) is used to avoid conflicts
- Interface creation and cleanup are handled automatically
- Safety checks ensure we don't accidentally remove real interfaces
- The interface name is tracked in `logs/current_test_iface`

## Running Tests

To run all tests in the correct order:
```bash
make
```

To run a specific test type:
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

## Logs

Test outputs are stored in:
- `tests/logs/` - General test logs
- `logs/` - System test vehicle logs

Unit and integration tests output directly to the console, while system tests redirect their output to log files to keep the console clean. 

## New Tests for P2 Component Architecture

### Unit Tests

#### Component Test

The `component_test` verifies the functionality of the non-templated `Component` base class introduced in the P2 architecture changes. It tests:

- Component creation through the Initializer
- Component lifecycle (start/stop)
- Communicator initialization and basic message sending
- Thread management

To run:
```bash
make run_unit_component_test
```

### Integration Tests

#### Memory Management Test

The `memory_management_test` verifies proper memory management of components when using `std::unique_ptr` in the Vehicle class. It tests:

- Component memory allocation and deallocation
- Vehicle component ownership
- Cleanup during vehicle destruction

This test can be run with Valgrind to check for memory leaks:
```bash
make run_integration_memory_management_test
make run_integration_memory_management_test_valgrind
```

Both tests are also automatically included when running:
```bash
make unit_tests        # Includes component_test
make integration_tests # Includes memory_management_test
# Or just run all tests with:
make
``` 