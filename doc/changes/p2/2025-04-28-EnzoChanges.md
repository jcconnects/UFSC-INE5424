---
title: "Buffer Handling and Shutdown Fixes"
date: 2025-04-29
time: "10:00"
---

# Buffer Handling and Shutdown Fixes

## Overview
This document outlines changes made to fix message corruption and a memory leak, and details a plan to resolve a segmentation fault occurring during test termination. The investigation followed issues noted in `2025-04-28-ResmerChanges.md`.

## 1. Changes Made During Investigation

### 1.1. Fixed Internal Message Corruption (`Protocol::send`)

*   **Problem:** When `NIC::send` routed a message internally, `Protocol::send` was freeing the shared `DataBuffer` immediately after `NIC::send` returned. However, `NIC::send` (via `handleInternalEvent` and `notify`) only initiated the reception; the receiving component processed the buffer later, accessing already freed memory. This caused the corrupted message content noted in Resmer's changes.
*   **Change (`include/protocol.h`):** Removed the `_nic->free(buf);` call from the end of `Protocol::send`.
    ```diff
    --- a/include/protocol.h
    +++ b/include/protocol.h
    @@ -227,9 +227,6 @@
         int result = _nic->send(buf);
         db<Protocol>(INF) << "[Protocol] NIC::send() returned value " << std::to_string(result) << "\n";
     
    -    // Releasing buffer
    -    // _nic->free(buf); // <-- REMOVED THIS LINE
    -
         return size;
     }
     
    ```
*   **Reason:** Buffer ownership for internal messages is now correctly transferred to the receiver via the observer pattern. The receiver (`Communicator::receive`) is responsible for freeing the buffer after processing.

### 1.2. Fixed Buffer Leak (`Communicator::receive`)

*   **Problem:** `Communicator::receive` obtained a `Buffer*` via the observer mechanism (`Observer::updated()`) but failed to free it using `_channel->free(buf)` in any code path (success, error, or exception) within its `try-catch` block after processing the buffer. This caused a memory leak for every received message.
*   **Change (`include/communicator.h`):** Added `_channel->free(buf);` before returning in the success case, the failure case (`size <= 0`), and the `catch` block.
    ```diff
    --- a/include/communicator.h
    +++ b/include/communicator.h
    @@ -188,16 +188,19 @@
                 if (source_address) {
                     *source_address = from;
                 }
    +            _channel->free(buf); // Free buffer on success
                 return true;
             }
     
             // If size <= 0, receive failed or returned no data
    +        _channel->free(buf); // Free buffer on failure
             return false;
         
         } catch (const std::exception& e) {
             db<Communicator>(ERR) << "[Communicator] Error receiving message: " << e.what() << "\n";
    +        _channel->free(buf); // Free buffer on exception
             return false;
         }
     }

    ```
*   **Reason:** Ensures buffers received via notification are always freed, preventing leaks.

### 1.3. Added `Protocol::free` Method (Fix Compilation)

*   **Problem:** The fix for the buffer leak in `Communicator::receive` involved calling `_channel->free(buf)`. However, the `_channel` points to a `Protocol` object, which did not have a `free` method. The `free` method belongs to the `NIC` class, which `Protocol` uses internally. This resulted in a compilation error.
*   **Change (`include/protocol.h`):** Added a public `free(Buffer* buf)` method to the `Protocol` class that forwards the call to its internal `_nic` pointer.
    ```diff
    --- a/include/protocol.h
    +++ b/include/protocol.h
    @@ -105,6 +105,9 @@
             static void attach(Observer* obs, Address address);
             static void detach(Observer* obs, Address address);
             
    +        // Add buffer free method
    +        void free(Buffer* buf);
    +
         private:
             void update(typename NIC::Protocol_Number prot, Buffer * buf) override;
     
    @@ -292,6 +295,17 @@
         }
     }
     
    +// Add implementation for Protocol::free
    +template <typename NIC>
    +void Protocol<NIC>::free(Buffer* buf) {
    +    if (_nic) {
    +        _nic->free(buf);
    +    } else {
    +        // Log error or handle case where NIC pointer is null (shouldn't happen in normal operation)
    +        db<Protocol>(ERR) << "[Protocol] free() called but _nic is null!\\n";
    +    }
    +}
    +
     // Initialize static members
     template <typename NIC>
     typename Protocol<NIC>::Observed Protocol<NIC>::_observed;

    ```
*   **Reason:** Fixes the compilation error by providing the necessary `free` method in the `Protocol` class interface, allowing `Communicator` to correctly free buffers via its `_channel` pointer.

## 2. Plan to Fix Termination Segfault

*   **Problem:** Tests like `virtual_dst_address_test` still segfault during termination (`v1->stop(); v2->stop(); ...`). The investigation identified a **deadlock** during component shutdown:
    1.  `Component::stop()` calls `pthread_join` to wait for the component's thread.
    2.  The component's thread is blocked inside `Component::receive` -> `Communicator::receive` -> `Concurrent_Observer::updated`, waiting on a semaphore (`_semaphore.wait()`).
    3.  `Component::stop` does not signal the `Communicator` to wake up the blocked thread.
    4.  `pthread_join` blocks indefinitely.
    5.  The main thread eventually proceeds (or times out), and the `Vehicle` destructor likely deletes the `Protocol` and `NIC` objects while component threads are still blocked or in an inconsistent state, leading to a use-after-free or other memory corruption when those threads eventually wake up or during cleanup, causing the segfault.

*   **Solution Steps:**

    1.  **Modify `Communicator::close`:** Ensure it safely signals the `Concurrent_Observer::update` mechanism to unblock waiting threads. Currently, it passes the address of a temporary stack variable, which is unsafe. Pass `nullptr` instead.
        *   **File:** `include/communicator.h`
        *   **Code Change:**
            ```diff
            --- a/include/communicator.h
            +++ b/include/communicator.h
            @@ -206,8 +206,8 @@
             
                 try {
                     // Signal any threads waiting on receive to wake up by calling update with nullptr
            -        Buffer buf = Buffer(); // REMOVED
            -        update(nullptr, _address.port(), &buf); // Pass address of temporary
            +        // Buffer buf = Buffer(); // REMOVED
            +        update(nullptr, _address.port(), nullptr); // Pass nullptr instead of address of temporary
                 } catch (const std::exception& e) {
                     std::cerr << "Error during communicator close: " << e.what() << std::endl;
                 }

            ```

    2.  **Modify `Component::stop`:** Call `_communicator->close()` *before* calling `pthread_join`. This will trigger the unblocking mechanism implemented in Step 1.
        *   **File:** `include/component.h`
        *   **Code Change:**
            ```diff
            --- a/include/component.h
            +++ b/include/component.h
            @@ -133,9 +133,9 @@
             void Component::stop() {
                 if (running()) {
                     _running.store(false, std::memory_order_release);
            -        // Optionally: Signal the communicator to interrupt blocking calls if implemented
            -        // if (_communicator) {
            -        //     _communicator->interrupt();
            -        // }
            +        // Signal the communicator to interrupt blocking calls
            +        if (_communicator) {
            +            _communicator->close(); // Close communicator BEFORE joining
            +        }
                     if (_thread != 0) {
                         int join_rc = pthread_join(_thread, nullptr);
                         if (join_rc == 0) {

            ```

*   **Expected Outcome:** Calling `_communicator->close()` will cause `Concurrent_Observer::update(..., nullptr)` to be called, posting the semaphore. The blocked `Concurrent_Observer::updated()` call will wake up, see the empty queue (because `nullptr` wasn't inserted), and return `nullptr`. `Communicator::receive` will detect this `nullptr` and the `_closed` flag, returning `false`. `Component::receive` will detect `!running()` and return `-1`. The component's `run()` loop will then exit, allowing `pthread_join` in `Component::stop` to complete successfully, resolving the deadlock and preventing the segfault during subsequent cleanup. 