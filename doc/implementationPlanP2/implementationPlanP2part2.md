---
title: "Implementation Plan P2 - Part 2: Consolidation"
date: 2025-04-25
---

# Implementation Plan P2 - Part 2: Consolidation

## Overview

This document outlines the necessary implementation steps to fully consolidate the Phase 2 (P2) architecture, building upon the per-component communicator refactoring and addressing the requirements for intra-vehicle communication using a dedicated mechanism. This plan incorporates the design decision to perform engine selection (Network vs. Local) at the `NIC` layer based on the destination physical MAC address.

## Implementation Steps

### 1. `NIC` Layer Modifications (`include/nic.h`)

The `NIC` class needs substantial modification to manage and multiplex between the network communication engine (`SocketEngine`) and the local communication mechanism (`SharedMemoryEngine` or its equivalent).

**A. Engine Management:**
   - The current template `NIC<Engine>` likely assumes a single engine type inherited privately (`private Engine`).
   - **Modify:** The `NIC` class should no longer inherit from `Engine`. Instead, it should *contain* instances or references to both required engines:
     ```cpp
     // Example members within NIC class (adjust types as needed)
     private:
         SocketEngine _network_engine; // Manages network socket comms
         SharedMemoryEngine _local_engine;  // Manages local comms (needs ComponentId)
         // Or, if SharedMemoryEngine remains static/global:
         // SharedMemoryEngine& _local_engine_ref; // Reference to a shared instance? - Revisit design
     ```
   - **Initialization:** The `NIC` constructor needs to initialize both engines. `SharedMemoryEngine` currently requires a `ComponentId`. Since the `NIC` serves the whole `Vehicle` (process), a dedicated ID (e.g., 0 or a special value) might be needed, or the `SharedMemoryEngine` design needs revisiting if it's meant to be used directly by the `NIC`. The `NIC` constructor currently gets the MAC from the inherited `Engine`; this needs to be sourced from `_network_engine` now.

**B. Sending Logic (`NIC::send`):**
   - The current `send(DataBuffer* buf)` calls `Engine::send()`.
   - **Modify:**
     ```cpp
     template </* No Engine Template Param */>
     int NIC::send(DataBuffer* buf) {
         if (!buf || !buf->data()) { /* error handling */ return -1; }

         Ethernet::Frame* frame = buf->data();
         const Address& dest_mac = frame->dst; // Get destination MAC from buffer

         // Check if destination MAC is the NIC's own MAC
         if (dest_mac == _address) { // _address is NIC's physical MAC
             db<NIC>(INF) << "[NIC] Routing frame locally via SharedMemoryEngine\n";
             // Option 1: Direct call if NIC owns/manages SharedMemoryEngine instance
             // Need to adapt SharedMemoryEngine::send signature if it expects Ethernet::Frame*
             // int result = _local_engine.send(frame, buf->size());

             // Option 2: If SharedMemoryEngine uses static methods/queues
             // Need a way to translate frame/buffer to SharedMemoryEngine input format
             // int result = SharedMemoryEngine::route_local(frame, buf->size()); // Hypothetical static method

             // Error handling and statistics update...
             return result;
         } else {
             db<NIC>(INF) << "[NIC] Routing frame to network via SocketEngine\n";
             int result = _network_engine.send(frame, buf->size());
             // Update network statistics...
             return result;
         }
     }
     ```
   - Ensure `_address` accurately holds the NIC's physical MAC address obtained from `_network_engine`.

**C. Receiving Logic (`NIC::handleSignal`, Event Handling):**
   - The current `handleSignal()` is tied to the inherited `Engine` (specifically `SocketEngine` via `recvfrom(this->_sock_fd, ...)`).
   - **Modify:** The `NIC` needs a unified event handling mechanism to process incoming data from *both* engines.
     - **Option 1 (NIC-level Epoll):** Introduce an `epoll` instance within the `NIC`. Register the `SocketEngine`'s socket FD (`_network_engine._sock_fd`) and the `SharedMemoryEngine`'s notification FD (`_local_engine._notify_fd`) with this `epoll` instance. The `NIC` would have its own `run()` method/thread waiting on this `epoll`. When an event occurs, it determines which engine has data, reads from it (using `recvfrom` for socket, or `SharedMemoryEngine::readFrame` for local), packages the data into a `DataBuffer`, and notifies the `Protocol` layer via `Observed::notify`.
     - **Option 2 (Polling/Callback - Less Ideal):** Periodically check both engines for data or use callbacks if the engines support them.
   - **Data Buffering:** The received data (network `Ethernet::Frame` or local `SharedMemoryEngine::SharedFrame`) needs to be consistently placed into a `NIC::DataBuffer` obtained via `alloc()`. The source/destination MAC addresses in the `DataBuffer`'s `Ethernet::Frame` must be set correctly based on the origin (real MACs for network, local MACs for local).
   - **Notification:** Call `Observed::notify(protocol, buffer)` to pass the received buffer up to the `Protocol` layer, regardless of the source engine.

**D. Buffer Management (`NIC::alloc`, `NIC::free`):**
   - Seems okay as it manages generic `DataBuffer`s. Ensure thread safety (`_binary_sem`, `_buffer_sem`) remains sufficient with potentially multiple input sources.

**E. Stop Logic (`NIC::stop`):**
   - **Modify:** Needs to explicitly stop *both* engines (`_network_engine.stop()`, `_local_engine.stop()`).

### 2. `Protocol` Layer Simplification (`include/protocol.h`)

With routing moved to `NIC`, the `Protocol` layer becomes simpler.

**A. Sending Logic (`Protocol::send`):**
   - Current logic allocates a buffer, prepares a `Packet` (with `Header` containing ports), and calls `_nic->send()`.
   - **Modify:** No significant change needed. It relies on `_nic->send()` to handle the routing based on the physical address (`to.paddr()`) embedded in the buffer.

**B. Receiving Logic (`Protocol::update` / `receive`):**
   - Current logic receives a buffer via `update` from the `NIC`, parses the `Packet`, and notifies observers (`Communicator`) based on the destination port.
   - **Modify:** No significant change needed. It remains agnostic to whether the buffer originated from the network or local engine, as long as the `NIC` provides a consistent `DataBuffer`.

**C. Template Parameter:**
   - The `Protocol<NIC>` template implies a tight coupling. If `NIC` is no longer templated on `Engine`, this template might become `Protocol` (non-templated) taking a `NIC*` in its constructor, or `Protocol<TheNIC>` if `NIC` itself becomes a concrete type.

### 3. Addressing Scheme (`include/protocol.h`, `include/vehicle.h`, `include/sharedMemoryEngine.h`)

The NIC-level routing simplifies address interpretation at the `Protocol` layer.

**A. `Protocol::Address`:**
   - No changes needed to the structure (`_paddr`, `_port`). No `is_local()` method is required.

**B. `SharedMemoryEngine` Address Generation:**
   - **Verify:** Ensure `SharedMemoryEngine` consistently generates local MACs (e.g., `0x02` prefix) based on `ComponentId`.

**C. `Vehicle::next_component_address` (`include/vehicle.h`):**
   - **Verify:** Ensure this method creates `Protocol::Address` instances where the `paddr` part uses the local MAC scheme generated by `SharedMemoryEngine` (or a compatible scheme). It should combine the vehicle's base *local* MAC and a unique component ID/port.

### 4. Message Format (`include/message.h`)

The message needs to carry origin information.

**A. Modify `Message<MaxSize>`:**
   - Add a member to store the origin address:
     ```cpp
     template <unsigned int MaxSize>
     class Message {
         // ... existing members ...
         TheAddress _origin; // Assuming TheAddress is the concrete Protocol::Address type
     public:
         // ... existing methods ...
         void origin(const TheAddress& addr) { _origin = addr; }
         const TheAddress& origin() const { return _origin; }
         // Update constructors/assignment operators if needed
     };
     ```

**B. Populate Origin (`include/communicator.h`):**
   - **Modify `Communicator::receive`:** After successfully receiving data via `_channel->receive(buf, from, ...)` (where `from` is populated by `Protocol`), store the `from` address in the message object before returning:
     ```cpp
     // Inside Communicator::receive, after getting data and 'from' address
     *message = Message<MAX_MESSAGE_SIZE>(temp_data, size);
     message->origin(from); // Set the origin address
     if (source_address) { *source_address = from; } // Keep old behaviour if needed
     return true;
     ```

### 5. `SharedMemoryEngine` Implementation (`include/sharedMemoryEngine.h`)

Address the discrepancy between the current implementation and the proposal's requirement for POSIX shared memory.

**A. Decide and Implement:**
   - **Option A (Refactor to POSIX Shared Memory):**
     - Replace static queues/mutexes with a shared memory segment (`shm_open`, `ftruncate`, `mmap`).
     - Implement a ring buffer or similar structure within the shared memory.
     - Use POSIX semaphores (`sem_open`, `sem_wait`, `sem_post`) or mutexes/condition variables initialized in shared memory (`pthread_mutexattr_setpshared`, etc.) for synchronization between processes/components (producers/consumers).
     - Re-evaluate the notification mechanism (`eventfd`) - might still be useful or could be replaced by semaphores.
     - This aligns strictly with the proposal but is more complex.
   - **Option B (Accept Current Implementation):**
     - Document that the current engine uses process-internal static queues and standard pthreads mutexes/eventfd for notification.
     - Acknowledge this deviates from the POSIX shared memory *mechanism* but achieves the *goal* of intra-vehicle (intra-process) communication efficiently for the current single-process vehicle model.
     - Choose this if the complexity of true shared memory is deemed unnecessary for the project scope.

**B. Interface Alignment:**
   - Ensure the `SharedMemoryEngine`'s `send`/`receive` (or equivalent) methods align with how the `NIC` needs to interact with it (e.g., passing `Ethernet::Frame` or similar data structure).
   - Clarify how the `NIC` obtains the `_notify_fd` for integration into its event loop (if Option 1 from 1.C is chosen).

### 6. Testing

- Develop new unit and integration tests specifically targeting:
    - Intra-vehicle communication between components.
    - Correct routing decisions within the `NIC::send` method (verify local vs. network paths).
    - Proper reception and demultiplexing of messages arriving via both `SocketEngine` and `SharedMemoryEngine`.
    - Correct population and retrieval of the `_origin` field in `Message` objects.

--- 