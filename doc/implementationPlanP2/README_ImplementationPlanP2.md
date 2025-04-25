# Operating Systems II - P2 Implementation Plan

This document outlines the plan for implementing the requirements of Project Stage 2 (P2), focusing on enabling communication between components within the same autonomous system (process) using shared memory, while maintaining a unified API with the existing network communication.

## 1. Core Goal

Extend the communication library to support:
- **Intra-System Communication:** Allow components (threads) within the same vehicle (process) to exchange messages.
- **Component Identification:** Uniquely identify source and destination components for all messages.
- **Unified API:** Maintain the existing `Communicator` API for both inter-system (network) and intra-system (shared memory) communication.
- **Transport Layer Routing:** Implement routing logic below the `Protocol` layer (likely within `NIC`) to select the appropriate transport mechanism (raw sockets or shared memory).

## 2. Addressing Scheme (`Protocol::Address`)

The existing `Protocol::Address` structure will be adapted to represent a unified address for both systems and components:

- **`Physical_Address _paddr`**: Continues to represent the System Identifier (Virtual MAC address of the vehicle/process). The broadcast MAC (`ff:ff:ff:ff:ff:ff`) retains its meaning for network broadcast.
- **`Port _port`**: **Re-purposed** to represent the **Component Identifier** (unique ID of the thread within the process).
    - A specific value (e.g., `0` or `Address::NULL_VALUE`) will designate "no specific component" or potentially "broadcast to all components" within the destination system. This needs a clear convention.

**Crucially, only `Component` instances will possess a `Communicator` and thus a full `Protocol::Address` representing a communication endpoint.** The `Vehicle` itself will not have a dedicated `Communicator`.

## 3. Packet Structure (`Protocol::Packet`)

The internal packet structure used by the `Protocol` layer needs to carry the full routing information:

- **`Protocol::Header`**: Modify to contain:
    - `Address _source_address;` (Full address of the originating component)
    - `Address _destination_address;` (Full address of the target component)
    - `unsigned int _size;` (Payload size, might be redundant if calculable)
- **`Protocol::Packet`**: Continues to embed the `Header` and the `Data` payload.

The application-facing `Message` class **remains unchanged**, containing only the payload. The `Protocol` layer will handle adding/removing the header during send/receive.

## 4. Engine Architecture & `NIC` Role

- **Engines:**
    - `SocketEngine`: Remains largely as is, handling raw Ethernet communication via sockets. It manages the `epoll` instance and the primary event loop thread (`SocketEngine::run`).
    - `SharedMemoryEngine`: Will manage shared memory segments, component-specific queues (or a shared queue with filtering), and synchronization primitives (semaphores/condition variables) for intra-process communication. It will provide an event mechanism (e.g., an `eventfd`) for the `NIC` layer to monitor when messages requiring protocol stack processing arrive via shared memory (if applicable).

- **`NIC` Layer (Inheritance & Composition):**
    - **Relationship:** The `NIC` class will continue to **inherit** (privately or protectedly) from `SocketEngine`, leveraging the existing P1 structure where `SocketEngine` provides the event loop (`run`) and calls a virtual method (`handleSignal`, to be renamed) implemented by `NIC`.
    - The `NIC` will also **contain an instance** of `SharedMemoryEngine` via composition.
    - **Reasoning:** This approach minimizes disruption by reusing the existing `SocketEngine` threading and `epoll` mechanism. The `NIC` extends this mechanism to monitor both network and shared memory events.
    - **Role:** Acts as the **central routing point** below the `Protocol`.
    - **Initialization:** Initializes the base `SocketEngine`, creates and initializes the `SharedMemoryEngine` member. Modifies the `epoll` setup (likely by overriding `SocketEngine::setUpEpoll` or adding a post-setup step) to include the event FD from the `SharedMemoryEngine` in the `epoll` set managed by the base `SocketEngine`.
    - **Buffer Management:** Continues to manage `DataBuffer` pool.
    - **`send(destination, buf)` Method:**
        - Receives the destination `Protocol::Address` and the `DataBuffer` (containing the `Protocol::Packet` with full headers) from `Protocol`.
        - **Inspects `destination.paddr()`:**
            - If `destination.paddr() == own_MAC_address`: Route locally. Calls `_sm_engine->send_local(...)`, passing the packet data. Frees `buf`.
            - If `destination.paddr() != own_MAC_address` (or is broadcast MAC): Route to network. Calls the base `SocketEngine::send(...)` method, passing the buffer containing the Ethernet frame. Frees `buf` after send attempt.
    - **Event Handling (within `SocketEngine::run` adaptation):**
        - The `SocketEngine::run` loop (or a method it calls) now checks which FD triggered `epoll_wait`:
            - If `_sock_fd`: Calls `handleNetworkPacket()` (new name for the overridden `handleSignal`).
            - If `_sm_event_fd`: Calls `handleSharedMemoryPacket()`.
    - **`handleNetworkPacket()`:** Reads the network frame, wraps it in a `DataBuffer`, determines `Ethernet::Protocol` number (`proto`), calls `notify(proto, buf)`.
    - **`handleSharedMemoryPacket()`:** Reads the shared memory event FD to clear the signal. Retrieves packet data from `_sm_engine`. Wraps the received *packet data* (not a full Ethernet frame) in a `DataBuffer` (e.g., placing it in the payload section). Calls `notify(Protocol::PROTO, buf)` using the **standard protocol number** defined in `Protocol` to signal the `Protocol` layer instance.

## 5. `Protocol` Layer

- Single instance per vehicle.
- Interacts solely with the `NIC` instance.
- **`send`:** Receives source/destination address and payload from `Communicator`. Creates the `Protocol::Packet` with full headers. Allocates `DataBuffer`. Places packet in buffer. Calls `_nic->send(destination, buf)`.
- **`receive`:** Called by `Communicator`. Retrieves `Packet` data from `NIC` (via buffer from `update`). Extracts `_source_address`. Copies payload to `Communicator`.
- **`update`:** Called by `NIC::notify`. Receives `DataBuffer`. Parses `Packet`. Extracts `_destination_address`. Uses `_destination_address.port()` (Component ID) to notify the correct `Communicator` via `_observed.notify(componentId, buf)`.

## 6. `Communicator` Layer

- One instance **per Component**. No `Communicator` for the `Vehicle`.
- Associated with a specific `Component` and its `ComponentID`/`Port`.
- Holds its own full `Protocol::Address`.
- Interacts only with the single `Protocol` instance (`_channel`).
- **`send(destination, message)`:** Calls `_channel->send(this->_address, destination, message->data(), message->size())`.
- **`receive(message)`:** Waits for its specific `update` notification from `Protocol` (keyed on its `ComponentID`/`Port`). Calls `_channel->receive(...)` to get payload. Can potentially return source address.
- **Observer:** Attaches to `Protocol::_observed` using its `_address.port()` as the condition.

## 7. `Component` Layer

- Each component thread gets a unique `ComponentID` (`Port`) upon registration (with `SharedMemoryEngine`).
- Stores its full `Protocol::Address`.
- **Owns its `Communicator` instance.**
- Uses its `Communicator` to `send` messages (providing full destination `Address`).
- Receives messages via its `Communicator`.

## 8. Initialization (`Initializer`/`Vehicle`)

- Instantiate `SharedMemoryEngine`.
- Instantiate `NIC` (which initializes its `SocketEngine` base and composed `SharedMemoryEngine`).
- Instantiate the single `Protocol`, providing pointer to the `NIC`.
- Create `Component` instances.
- Register components with `SharedMemoryEngine` to get their `ComponentID`/`Port` and corresponding `Protocol::Address`.
- Create a `Communicator` instance **for each component**, passing the `Protocol*` and the component's full `Address`. Store the `Communicator` within the `Component`.

## 9. Diagram

Refer to `ImplementationPlanP2.puml` for a visual representation of the class relationships and interactions. 