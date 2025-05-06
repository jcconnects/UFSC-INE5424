---
title: "NIC Refactoring for Dual Engine Support (P2 Consolidation)"
date: 2025-04-26
---

# NIC Refactoring for Dual Engine Support (P2 Consolidation)

## Overview

This refactoring modifies the `NIC` (Network Interface Card) class to support the Phase 2 architecture consolidation plan. The primary goal is to enable the `NIC` to manage two distinct communication engines simultaneously: one for external network communication (e.g., `SocketEngine`) and one for internal intra-vehicle communication (e.g., `SharedMemoryEngine`). This allows the `NIC` layer itself to route outgoing traffic to the appropriate engine based on the destination MAC address and to provide a unified reception path for incoming data from both engines to the upper `Protocol` layer.

## Changes

The following changes were implemented in `include/nic.h`:

1.  **Dual Engine Template Parameters:**
    -   The `NIC` class template signature was changed from `template <typename Engine>` to `template <typename ExternalEngine, typename InternalEngine>`.

2.  **Engine Management:**
    -   `NIC` no longer inherits privately from a single `Engine`.
    -   It now holds instances of both engine types as private member variables: `ExternalEngine _external_engine;` and `InternalEngine _internal_engine;`.

3.  **Unified Event Loop:**
    -   `NIC` now manages its own event loop using `epoll` instead of relying on an inherited engine's loop.
    -   Added private members: `_nic_ep_fd` (epoll instance), `_stop_event_fd` (for shutdown signaling), `_event_thread` (pthread for the loop), and `_running` (atomic flag).
    -   Implemented a static `eventLoop(void* arg)` method that runs in `_event_thread`. This loop waits on `_nic_ep_fd`.
    -   The `epoll` instance monitors the `_stop_event_fd` and notification file descriptors obtained from *both* `_external_engine` and `_internal_engine` via a required `getNotificationFd()` method in the engines.

4.  **Constructor (`NIC::NIC`)**:
    -   Initializes both `_external_engine` and `_internal_engine`.
    -   Retrieves the NIC's primary MAC address by calling `_external_engine.getMacAddress()` and stores it in `_address`.
    -   Initializes the buffer pool and semaphores as before.
    -   Creates the `_stop_event_fd`.
    -   Calls a new private helper `setupNicEpoll()` to create the `epoll` instance and register the stop FD and the notification FDs from both engines.
    -   Starts the `_event_thread` to execute `eventLoop`.

5.  **Sending Logic (`NIC::send`)**:
    -   Retrieves the destination MAC address from the `DataBuffer`.
    -   **Routing Decision:**
        -   If `destination MAC == NIC's _address`, the frame is routed via `_internal_engine.send()`.
        -   Otherwise (destination MAC is different), the frame is routed via `_external_engine.send()`.
    -   Updates separate internal/external statistics accordingly.

6.  **Receiving Logic (Event Handlers):**
    -   Removed the `handleSignal()` override.
    -   Added private methods `handleExternalEvent()` and `handleInternalEvent()`, called by `eventLoop` when the respective engine's notification FD is ready.
    -   `handleExternalEvent()`: Calls a receive method on `_external_engine` (e.g., `receiveFrame`), performs validation, allocates a `DataBuffer`, copies the received `Ethernet::Frame`, updates external statistics, and notifies the `Protocol` layer via `Observed::notify(protocol_number, buffer)`.
    -   `handleInternalEvent()`: Calls a receive method on `_internal_engine` (e.g., `receiveData`), **reconstructs** a standard `Ethernet::Frame` (using the NIC's `_address` for both source and destination MACs, and the protocol number provided by the internal engine), allocates a `DataBuffer`, copies the reconstructed frame, updates internal statistics, and notifies the `Protocol` layer via `Observed::notify(protocol_number, buffer)`. This ensures the `Protocol` layer always receives a consistent `DataBuffer` containing an `Ethernet::Frame`, regardless of the origin.

7.  **Stop Logic (`NIC::stop`)**:
    -   Sets the `_running` flag to false.
    -   Writes to `_stop_event_fd` to unblock the `eventLoop`.
    -   Calls `stop()` on both `_external_engine` and `_internal_engine` (assuming they provide this method).
    -   Joins the `_event_thread`.
    -   Cleans up FDs (`_nic_ep_fd`, `_stop_event_fd`).

8.  **Other Method Changes:**
    -   `address()`: Returns the primary MAC address (`_address`) obtained from the external engine.
    -   `setAddress()`: Removed, as the address is now determined by the external engine.
    -   `statistics()`: Returns the `Statistics` struct (which now has separate internal/external counters).
    -   `alloc()`: Sets the source MAC in the allocated buffer's frame header to `_address`.
    -   `receive()`: Primarily serves to unwrap the payload from a `DataBuffer` for the `Protocol` layer after it has received the buffer via the `notify` mechanism.

## Engine Interface Requirements

For an engine class to be used as either `ExternalEngine` or `InternalEngine` with this refactored `NIC`, it must provide the following conceptual interface (specific method names may vary):

-   `Ethernet::Address getMacAddress()`: Returns the MAC address associated with this engine. (Needed primarily by `ExternalEngine`).
-   `int getNotificationFd()`: Returns a readable file descriptor used to signal the `NIC` when data is ready to be received.
-   `int send(Ethernet::Frame* frame, unsigned int size)`: Sends the provided Ethernet frame data. Returns bytes sent or < 0 on error.
-   `int receiveFrame(Ethernet::Frame& frame_buffer)` / `int receiveData(...)`: Method(s) called by the NIC's event handlers (`handleExternalEvent`/`handleInternalEvent`) to retrieve incoming data after being notified. Must provide the received data and necessary metadata (like the Ethernet protocol number for internal data). Returns bytes received or < 0 on error/no data.
-   `void stop()`: Performs any necessary cleanup or stopping of background tasks for the engine.
-   `bool running()`: Returns true if the engine is operational (optional, but helpful).
-   **(Trait) `MTU`:** A way to determine the Maximum Transmission Unit for payload (used by `handleInternalEvent` for buffer sizing).

## Adapting SocketEngine (as ExternalEngine)

To make `SocketEngine` compatible with the refactored `NIC`, the following changes were made in `include/socketEngine.h`:

1.  **Removed Internal Event Loop:**
    -   Removed the `_ep_fd` (epoll instance), `_stop_ev` (eventfd), and `_receive_thread` members.
    -   Removed the `setUpEpoll()` and `run()` methods.
    -   The engine no longer manages its own thread or epoll instance; event notification relies on the NIC monitoring the engine's socket FD.

2.  **Simplified `start()` / `stop()`:**
    -   `start()` now only calls `setUpSocket()` (which creates, configures, and binds the raw socket) and sets the `_running` flag. It no longer creates a thread or epoll instance.
    -   `stop()` now only closes the `_sock_fd` if it's valid and sets the `_running` flag to false. It no longer signals or joins a thread.

3.  **Implemented `receiveFrame()`:**
    -   Added the method `int receiveFrame(Ethernet::Frame& frame_buffer)`. This method is intended to be called by the `NIC::handleExternalEvent()` when the NIC's epoll loop detects readability on the socket FD.
    -   It performs the `recvfrom()` call directly on `_sock_fd`, receiving data into the provided `frame_buffer`.
    -   It handles non-blocking behavior (`EAGAIN`/`EWOULDBLOCK`), returning 0 in that case.
    -   It performs basic validation (minimum size).
    -   It converts the protocol field (`frame_buffer.prot`) from network to host byte order (`ntohs`) before returning.
    -   Returns the number of bytes received on success, 0 if no data is available, or a negative value on error.

4.  **Removed `handleSignal()`:** The logic previously in this method is now encapsulated within `receiveFrame()`.

5.  **Added Interface Methods:**
    -   Added `Ethernet::Address getMacAddress() const`: Returns the `_mac_address` obtained during `setUpSocket()`.
    -   Added `int getNotificationFd() const`: Returns the raw socket descriptor `_sock_fd`. The `NIC` will add this FD to its own epoll instance to detect incoming packets.

6.  **Minor Adjustments:**
    -   Made `running()` method `const`.
    -   Removed virtual destructor as it's no longer a base class for `NIC`.
    -   Added error handling and null termination checks in `setUpSocket`.
    -   Ensured `frame->prot` is converted back to host order immediately after `sendto` in the `send` method for consistency.

## Refactoring SharedMemoryEngine (as InternalEngine)

The `SharedMemoryEngine` was substantially refactored in `include/sharedMemoryEngine.h` to use POSIX shared memory and semaphores, fulfilling the P2 requirements and meeting the `InternalEngine` interface:

1.  **POSIX Shared Memory:**
    -   Replaced the previous static map/mutex/eventfd approach.
    -   Uses `shm_open` to create/open a named shared memory segment (`/vehicle_internal_shm`).
    -   Uses `ftruncate` (by the first initializer process) to set the size.
    -   Uses `mmap` to map the segment into the process address space.
    -   Defines a `SharedRegion` struct mapped into this memory, containing:
        -   An `initialized` flag.
        -   A `ref_count` atomic integer for process attachment tracking.
        -   A fixed-size ring buffer (`SharedFrameData buffer[QUEUE_CAPACITY]`) where `QUEUE_CAPACITY` comes from `Traits<SharedMemoryEngine>::BUFFER_SIZE`.
        -   `read_index` and `write_index` for the ring buffer.
        -   The `SharedFrameData` struct holds the `Ethernet::Protocol` number and the actual payload (`unsigned char payload[MTU]`) plus its size.

2.  **POSIX Named Semaphores:**
    -   Uses `sem_open` to create/open three named semaphores:
        -   `/vehicle_shm_mutex`: Mutex (binary semaphore, initial value 1) for exclusive access to the `SharedRegion` data structures (indices, buffer).
        -   `/vehicle_shm_items`: Counting semaphore (initial value 0) incremented when an item is added, decremented when removed.
        -   `/vehicle_shm_space`: Counting semaphore (initial value `QUEUE_CAPACITY`) incremented when an item is removed, decremented when added.
    -   Uses `sem_wait` (or `sem_trywait` in receive) and `sem_post` for synchronization.
    -   Uses `sem_close` on destruction/stop.
    -   Uses `sem_unlink` and `shm_unlink` during the cleanup (`stop()`) when the last process detaches (tracked by `ref_count`).

3.  **Initialization and Attachment:**
    -   The `start()` method handles both initial creation (first process) and attachment (subsequent processes).
    -   The first process creates SHM, truncates, maps, initializes the `SharedRegion` struct, creates and initializes semaphores, and sets the `initialized` flag.
    -   Subsequent processes open existing SHM, map it, open existing semaphores, wait for the `initialized` flag, and increment the `ref_count`.

4.  **Timerfd Notification:**
    -   Removed the per-component `eventfd` and associated logic.
    -   The engine now creates a periodic `timerfd` (`_poll_timer_fd`) in its `start()` method.
    -   The interval is configurable via `Traits<SharedMemoryEngine>::POLL_INTERVAL_MS`.
    -   `getNotificationFd()` returns this `_poll_timer_fd`.
    -   The `NIC`'s event loop monitors this timerfd. When it triggers, the `NIC` calls `SharedMemoryEngine::receiveData`.

5.  **Sending Logic (`send`)**:
    -   Waits for space using `sem_wait(_space_sem)`.
    -   Waits for mutex using `sem_wait(_mutex_sem)`.
    -   Copies the `Ethernet::Protocol` and payload from the input `Ethernet::Frame` into the next available slot in the shared ring buffer.
    -   Updates the `write_index`.
    -   Releases the mutex using `sem_post(_mutex_sem)`.
    -   Signals item availability using `sem_post(_items_sem)`.
    -   Returns payload size on success, negative on error.

6.  **Receiving Logic (`receiveData`)**:
    -   Called by `NIC::handleInternalEvent` after the timerfd triggers.
    -   Uses `sem_trywait(_items_sem)` to check if an item is actually available without blocking indefinitely (as the timer is just a periodic poll).
    -   If an item is available (`sem_trywait` succeeds):
        -   Waits for mutex using `sem_wait(_mutex_sem)`.
        -   Reads the `Ethernet::Protocol` and payload from the ring buffer at the `read_index`.
        -   Copies the protocol and payload to the output parameters provided by the NIC.
        -   Updates the `read_index`.
        -   Releases the mutex using `sem_post(_mutex_sem)`.
        -   Signals space availability using `sem_post(_space_sem)`.
    -   Returns payload size read, 0 if no item was ready (`sem_trywait` returned EAGAIN), or negative on error.

7.  **Removed Component Logic:**
    -   Removed `ComponentId`, local MAC address generation, static maps (`_notify_fds`, `_component_queues`), and global/queue mutexes.
    -   The engine is now a single entity serving the whole process.

8.  **Cleanup (`stop`, `cleanupSharedResources`)**:
    -   `stop()` calls `cleanupSharedResources`.
    -   `cleanupSharedResources` closes semaphores, unmaps shared memory, closes the SHM fd, closes the timer fd, and decrements the reference count.
    -   If the reference count reaches zero, it calls `sem_unlink` and `shm_unlink` to remove the named resources from the system.

## Integration Updates

Following the refactoring of `NIC`, `SocketEngine`, and `SharedMemoryEngine`, several other classes were updated to use the new dual-engine `NIC` type correctly:

1.  **`Traits<SharedMemoryEngine>` (`include/traits.h`):**
    -   Added a specialization `Traits<SharedMemoryEngine>`.
    -   Defined constants required by the new `SharedMemoryEngine` implementation:
        -   `BUFFER_SIZE`: Capacity of the shared memory ring buffer (e.g., 128).
        -   `POLL_INTERVAL_MS`: Interval for the `timerfd` notification (e.g., 10ms).
        -   `MTU`: Maximum payload size for shared frames (e.g., 1500).

2.  **Type Aliases (`include/component.h`):**
    -   Updated the central type aliases used throughout the system:
        -   `using TheNIC = NIC<SocketEngine, SharedMemoryEngine>;`
        -   `using TheProtocol = Protocol<TheNIC>;`
        -   `using TheAddress = TheProtocol::Address;`
        -   `using TheCommunicator = Communicator<TheProtocol>;`
        -   `using TheMessage = Message<TheCommunicator::MAX_MESSAGE_SIZE>;`
    -   Included `sharedMemoryEngine.h`.

3.  **`Vehicle` (`include/vehicle.h`):**
    -   Replaced all occurrences of `NIC<SocketEngine>` and `Protocol<NIC<SocketEngine>>` with the type aliases `TheNIC` and `TheProtocol` respectively.
    -   Updated constructor signature, member variable types (`_nic`, `_protocol`, `_base_address`), and method return types (`address()`, `protocol()`, `next_component_address()`).
    -   Updated `MAX_MESSAGE_SIZE` constant to use `TheCommunicator`.
    -   Improved logging and added null checks/exception handling in constructor/destructor.

4.  **`Initializer` (`include/initializer.h`):**
    -   Updated the `VehicleNIC` and `CProtocol` type aliases to use `TheNIC` and `TheProtocol`.
    -   Modified `create_vehicle` to directly instantiate `new VehicleNIC()` (the dual-engine NIC) and `new CProtocol(nic)`.
    -   Removed the no-longer-needed manual setting of the NIC address (`nic->setAddress(...)`) as the NIC now derives its address from the `SocketEngine` internally.
    -   Included `sharedMemoryEngine.h`.

## Message Origin Tracking

To allow receivers to identify the original sender of a message (as required by P2), the following changes were made:

1.  **`Message` (`include/message.h`):**
    -   **Fixed Data Storage:** Corrected a significant bug where `_data` was `void* _data[MaxSize]` instead of `unsigned char _data[MaxSize]`. The implementation was updated accordingly.
    -   Added a private member `_origin` of type `TheAddress`.
    -   Added public `const TheAddress& origin() const` getter.
    -   Added public `void origin(const TheAddress& addr)` setter.
    -   Constructors and assignment operator were updated to initialize/copy the `_origin` member.

2.  **`Communicator` (`include/communicator.h`):**
    -   In the `receive` method, after the call to `_channel->receive(buf, from, ...)` successfully retrieves data and the source `Address` into the `from` variable, the code now sets the origin on the message object:
        ```cpp
        *message = Message<MAX_MESSAGE_SIZE>(temp_data, size);
        message->origin(from); // Set the origin address
        ```

## Impact on Protocol Layer

The refactoring of the `NIC` layer has minimal direct impact on the `Protocol` class (`include/protocol.h`) implementation logic itself, primarily affecting how it's instantiated and how it interacts with the `NIC`'s observation mechanism:

1.  **Type Instantiation:** Code using the `Protocol` class now uses the `TheProtocol = Protocol<TheNIC>` type alias, where `TheNIC` is the dual-engine `NIC<SocketEngine, SharedMemoryEngine>`. This change is handled centrally via the type aliases (e.g., in `component.h`).
2.  **Observer Inheritance:** `Protocol` now inherits directly from `NIC::Observer` (specifically, `Conditional_Data_Observer<Buffer<Ethernet::Frame>, Ethernet::Protocol>`) and observes the `NIC` for a specific Ethernet protocol number (`PROTO`). This replaces the previous indirect observation of a single engine.
3.  **Consistent Reception Interface (`Protocol::update`)**: The most significant aspect for `Protocol` is that the refactored `NIC` *abstracts away the origin* of incoming data (internal vs. external). The `NIC` guarantees that the `Protocol::update(Protocol_Number prot, DataBuffer* buf)` method **always** receives a `DataBuffer` containing a standard `Ethernet::Frame`.
    *   For data from `ExternalEngine` (`SocketEngine`), the `NIC` passes the received frame directly.
    *   For data from `InternalEngine` (`SharedMemoryEngine`), the `NIC` reconstructs a standard `Ethernet::Frame` (using the NIC's own MAC address for source/destination) before allocating the `DataBuffer` and notifying `Protocol`.
    *   This ensures the `Protocol` layer doesn't need to differentiate between internal and external sources; it consistently works with `Ethernet::Frame` objects within `DataBuffer`s.
4.  **Reception Logic:** The logic within `Protocol::update` remains the same: extract the `Packet` (containing the `Header`) from the buffer's payload, determine the destination `Port` from the header, and notify the appropriate higher-level observer (e.g., `Communicator`) attached to that port using `_observed.notify(dst_port, buf)`. The `Protocol` layer is responsible for managing observers based on its `Port` concept.
5.  **Sending Logic (`Protocol::send`)**: No changes were required here. `Protocol::send` continues to allocate a buffer using `_nic->alloc()` (providing destination MAC, protocol, size) and sends it using `_nic->send()`. The `NIC` now handles the routing decision internally based on the destination MAC provided during allocation/sending.
6.  **Unwrapping Logic (`Protocol::receive`)**: This method, typically called by `Communicator` after receiving a buffer from the `update` mechanism, remains unchanged. It uses `_nic->receive()` to unwrap the payload from the `DataBuffer` it was given.

In summary, the `Protocol` layer benefits from the `NIC` refactoring by receiving a consistent view of incoming data regardless of the underlying communication engine, simplifying its interaction with the network layer. Its core responsibilities of header processing, port-based demultiplexing, and managing observers remain the same.