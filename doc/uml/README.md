# UML Diagrams for UFSC-INE5424

This directory contains UML diagrams for the UFSC-INE5424 communication system.

## System Diagrams

1. **[System Sequence Diagram](01-system_sequence_diagram.puml)**
   - Shows the overall sequence of communication between components
   - Illustrates message flow through the communication stack

2. **[Process/Thread Diagram](02-process_thread_diagram.puml)**
   - Shows the process and thread architecture of the system
   - Illustrates how components run in separate processes and threads

3. **[Class Diagram](03-class_diagram.puml)**
   - Shows the main classes and their relationships
   - Provides an overview of the system architecture

4. **[Use Case Diagram](04-use_case_diagram.puml)**
   - Shows the use case diagram of the system
   - Illustrates the interactions between the components

## Component Diagrams

Individual components have their own UML diagrams:

- **[Buffer](buffer.puml)** - Memory management for network data
- **[Ethernet](ethernet.puml)** - Ethernet frame handling and MAC address management
- **[Message](message.puml)** - Generic container for communication data
- **[NIC](nic.puml)** - Network interface card implementation
- **[Protocol](protocol.puml)** - Protocol implementation for message routing
- **[SocketEngine](socketEngine.puml)** - Low-level network access

## Test Framework Diagrams

The test framework is represented in the following diagrams:

1. **[Test Organization](test_organization.puml)**
   - Shows the organization of tests into unit, integration, and system levels
   - Illustrates the relationship between different test components

2. **[Test Dependencies](test_dependencies.puml)**
   - Shows how tests depend on each other
   - Illustrates the execution flow from unit tests to system tests

3. **[Interface Handling](interface_handling.puml)**
   - Demonstrates how the network interface is managed for tests
   - Shows the safety mechanisms for interface creation and cleanup

For detailed diagrams of individual test components, see the [tests](tests/) directory.

## Generating Diagrams

Use Visual Studio Code extension or online tools.