Below is the English translation of the provided Portuguese document, structured and formatted for clarity based on the user's query.

---

This document presents the project pattern proposal for the Operating Systems II course in the first semester of 2025. The proposal revolves around the development of a reliable and secure communication library for critical autonomous systems, set in the scenario of autonomous vehicles.

### Global Project Requirements

- **Development Environment**: The development must be done in the C++ programming language using only the C Standard Library (libc) and the C++ Standard Library on a native POSIX platform.
- **Autonomous System Modeling**: Each autonomous system (e.g., vehicle) must be modeled as a macro-object associated with a specific POSIX process (and not a specific virtual machine or single-board computer).
- **Component Modeling**: Each component of an autonomous system (e.g., sensor, fuser, machine learning model) must be modeled as a macro-object associated with a POSIX thread (and not a process).
- **Network Communication**: Communication over the network will always occur via broadcast, with its reach limited by the inherent collision domain of the respective technology (e.g., a 5G radio cell or an Ethernet local network).
- **Group Composition**: Groups will consist of up to 4 students and may include students from both classes, provided they are present at all project progress presentations and the final presentation.
- **Submission Process**: After developing and testing each project stage, groups must submit, via Moodle, a link to a specific commit in their own Git repository. All specifications and project diagrams must be located in a folder named "doc". Along with the link, groups must provide access credentials so evaluators can access the code (do not use an open repository; you can use https://codigos.ufsc.br/). At the root of the development tree, include a Makefile capable of triggering the compilation and execution of all tests with the simple command `make`. Slides used in progress presentations for each project stage can either be placed in the "doc" folder in Git or in an online document referenced by a second URL provided in the Moodle task. Presentations must include a simple performance evaluation with the observed average latency during tests.

---

### 1. Communication Between Systems and Their Components

The communication library must provide a unified API for all agents, whether they are autonomous systems or their components. Messages exchanged between agents have a known maximum size, smaller than the network's Maximum Transmission Unit (MTU), ensuring they are never fragmented.

#### Asynchronous Event Propagation

Asynchronous events from the communication protocol stack being developed in this project must be managed using the Observer X Observed design pattern. Observed entities will notify their observers to update their states. Specifically, message reception must always be asynchronous, without the use of rendezvous protocols.

#### Engine for Communication Between Autonomous Systems

The `Engine` class, as specified above, must be implemented using raw sockets with Ethernet frames. Programming languages with a level of abstraction from the computational platform no higher than this may be considered.

#### Messages

At this stage, messages are defined as a simple byte array:

```
M = {.*}
```

#### API

Both autonomous systems and their components will communicate using the same set of basic primitives, adhering to a unified API.

---

### 2. Communication Between Components of the Same Autonomous System

In this stage, the API developed in the previous stage for communication between autonomous systems must be extended to support communication between components of the same system. All differentiation must be confined to the `Engine` class referenced in the API specification. This second `Engine` must be implemented using shared memory in a Producer X Consumer model, synchronized with the appropriate `pthreads` primitives. Additionally, the full identification of communication actors must now be modeled and implemented.

#### Identifiers

The complete identification of communication actors is essential for the proper functioning of the library being developed and can be achieved in various ways. However, several key points require attention:

- **NIC Addresses**: Addresses in the `NIC` class (i.e., `NIC::Address`) are Ethernet addresses (also known as MAC addresses). Note that a single machine with multiple network interface cards (NICs) will have multiple such addresses. Furthermore, project requirements mandate that the destination address be a broadcast address.
- **Protocol Addresses**: Addresses in the `Protocol` class (i.e., `Protocol::Address`) enable multiplexing of NICs to support multiple protocol sessions and must be unique in this context. Using `NIC::Address` directly as `Protocol::Physical_Address` and process IDs (PIDs) as `Protocol::Port` is an appealing option, as this combination generates globally unique identifiers with a clear and computationally inexpensive formation rule. However, this approach raises a question: how would it be applied to identify components of the same system communicating via shared memory?
- **Communicator Class**: The `Communicator` class does not employ explicit addressing, delegating the identification of message origins and destinations to the messages themselves or other entities within the communication stack. Nevertheless, as will be evident in stage 3, the receiver of a message must be able to identify the necessary entities to respond to it.

#### Messages

At this stage, messages must include, in addition to the byte array (now termed the "payload"), at least information about their origin to enable responses:

M = {origin, payload}
