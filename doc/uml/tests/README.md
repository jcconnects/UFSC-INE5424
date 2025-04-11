# Test UML Diagrams

This directory contains UML diagrams for the test components of the UFSC-INE5424 communication system.

## Test Organization Diagrams

The test organization and structure is represented in the following diagrams:

1. **[Test Organization Structure](../test_organization.puml)**
   - Shows the overall organization of tests into unit, integration, and system levels
   - Illustrates the relationship between different test components
   - Displays the build system and logging infrastructure

2. **[Test Dependencies Structure](../test_dependencies.puml)**
   - Shows how tests depend on each other
   - Illustrates the execution flow from unit tests to system tests
   - Demonstrates how higher-level tests build on lower-level ones

3. **[Interface Handling](../interface_handling.puml)**
   - Demonstrates how the network interface is managed for tests
   - Shows the safety mechanisms to protect real network interfaces
   - Illustrates the dynamic interface name resolution process

## Individual Test Diagrams

Each component test has its own UML diagram:

### Unit Tests

- **[buffer_test.puml](buffer_test.puml)** - Tests for the Buffer component
- **[ethernet_test.puml](ethernet_test.puml)** - Tests for the Ethernet component
- **[list_test.puml](list_test.puml)** - Tests for the List component
- **[message_test.puml](message_test.puml)** - Tests for the Message component
- **[observer_pattern_test_class.puml](observer_pattern_test_class.puml)** - Class diagram for Observer pattern tests
- **[observer_pattern_test_sequence.puml](observer_pattern_test_sequence.puml)** - Sequence diagram for Observer pattern tests
- **[socketEngine_test.puml](socketEngine_test.puml)** - Tests for the SocketEngine component

### Integration Tests

- **[initializer_test.puml](initializer_test.puml)** - Tests for the Initializer component
- **[nic_test.puml](nic_test.puml)** - Tests for the NIC component
- **[protocol_test.puml](protocol_test.puml)** - Tests for the Protocol component
- **[vehicle_test.puml](vehicle_test.puml)** - Tests for the Vehicle component

## Generating Diagrams

To generate PNG images from these PlantUML files, use the following command:

```bash
java -jar plantuml.jar -o output test_organization.puml test_dependencies.puml interface_handling.puml
```

The generated images will be stored in the `output` directory. 