# Changes Summary - 2025-04-29

This document outlines the changes implemented to add internal broadcast functionality and related protections.

## Feature: Internal Broadcast via Port 0

- **Goal:** Allow components within the same simulated vehicle to send broadcast messages to all other components using a designated port (Port 0).

### Implementation Details:

1.  **Broadcast Port:** Port 0 is designated as the internal broadcast port. Messages sent to this port via the `Component::send` method (using the appropriate `TheAddress` with port 0) are intended for all local components.

2.  **Observer Logic (`include/observer.h`):**
    *   Modified `Concurrent_Observer::update` to accept messages if the notification condition `c` matches the observer's rank *or* if `c` is the broadcast condition (0). This ensures observers listening on specific ports also receive broadcast messages.

3.  **Observed Logic (`include/observed.h`):**
    *   Modified `Concurrent_Observed::notify` (used by `Protocol` to notify `Communicator` instances based on port).
    *   If the notification condition `c` (the destination port) is 0, the method now iterates through *all* attached observers (all `Communicator` instances attached to the `Protocol`) and calls their `update` method with condition 0.
    *   If `c` is not 0, the original behavior (notifying only the observer matching the specific port `c`) is maintained.

4.  **External Broadcast Prevention (`include/protocol.h`):**
    *   Added a check within `Protocol::update`. Before notifying observers, this check examines incoming packets received from the NIC.
    *   If a packet's source MAC address (`frame->src`) does *not* match the NIC's own MAC address (meaning it originated externally) AND the packet's destination port (`packet->header()->to_port()`) is 0, the packet is dropped, and `_nic->free(buf)` is called.
    *   This prevents external network traffic from utilizing the internal broadcast mechanism.

### Affected Files:

- `include/observer.h`: Modified `Concurrent_Observer::update`.
- `include/observed.h`: Modified `Concurrent_Observed::notify`.
- `include/protocol.h`: Modified `Protocol::update`.

## Buffer Management Improvements (`include/protocol.h`)

- **Fixed Buffer Leak:** Modified `Protocol::send` to ensure that the NIC buffer allocated via `_nic->alloc` is correctly freed using `_nic->free` if the subsequent call to `_nic->send` fails. Previously, failed sends would leak the allocated buffer.
- **Corrected Allocation Size:** Adjusted the size calculation passed to `_nic->alloc` in `Protocol::send` to provide the *total Ethernet frame size* (Ethernet Header + Protocol Header + Data Payload), matching the expectation of the `NIC::alloc` implementation.
- **Added MTU Check:** Included a check in `Protocol::send` before allocation to prevent attempting to send frames larger than the NIC's advertised MTU.
- **Set Header Size Field:** Explicitly set the `_size` field in the `Protocol::Header` within `Protocol::send` to match the size of the user data payload being sent. 

## Concurrency and Robustness Fixes

- **Component Shutdown:** Modified `Component::stop` (`include/component.h`) to call `_communicator->close()` before `pthread_join`. This ensures that any component thread blocked in `_communicator->receive()` (waiting on the underlying semaphore) is properly unblocked, preventing potential deadlocks during shutdown.
- **Communicator Closing Signal:** Modified `Communicator::close` (`include/communicator.h`) to signal its observer with `nullptr` as the data argument. This provides a clearer signal to the waiting `receive` call that the communicator is closing, rather than relying on the receiver to check for an empty queue after being woken up.
- **NIC Allocation during Shutdown:** Added a check in `NIC::alloc` (`include/nic.h`) immediately after acquiring the buffer queue semaphore (`sem_wait`) to verify if the NIC is still running. This prevents a race condition where `alloc` could proceed after the NIC has been signaled to stop but before the semaphore wait completes.
- **Linker Fix for Ethernet::MTU:** Added an explicit definition (`constexpr unsigned int Ethernet::MTU;`) outside the class definition in `include/ethernet.h`. This helps resolve potential linker errors related to undefined references to the static constexpr `MTU` member, particularly in some toolchain configurations.

### Affected Files:

- `include/component.h`: Modified `Component::stop`.
- `include/communicator.h`: Modified `Communicator::close`.
- `include/nic.h`: Modified `NIC::alloc`.
- `include/ethernet.h`: Added explicit definition for `Ethernet::MTU`.

## Outstanding Issues & Further Investigation

- **Segmentation Fault in `virtual_dst_address_test`:** Despite concurrency and linker fixes, this test still encounters a segmentation fault during execution. Further debugging (e.g., using `gdb`) is required.
- **Potential Causes for Segfault:** Based on code review, the investigation should prioritize:
    1.  **`Protocol::receive` Logic:** The current implementation calls `_nic->receive` unnecessarily on a buffer already processed by the NIC. This double-handling might corrupt data or lead to invalid memory access. `Protocol::receive` should likely be refactored to directly interpret the buffer it receives as an argument.
    2.  **Buffer Lifecycle Issues:** The complex handoff of `DataBuffer` pointers between NIC, Protocol, and Communicator layers via the observer pattern increases the risk of use-after-free errors, especially if `Protocol::update` frees a buffer that is still referenced elsewhere.
    3.  **Memory Copying (`memcpy`) Bounds:** Verify size calculations and buffer boundaries for all `memcpy` operations in the send and receive paths (`Protocol::send`, `Protocol::receive`, `Component::receive`).
    4.  **Test Addressing:** Confirm that `ECU2_PORT` used in `virtual_dst_address_test.cpp` correctly matches the port assigned to the target ECU component by the `Initializer`. 