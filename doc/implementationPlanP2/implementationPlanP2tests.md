---
title: "Implementation Plan P2 - Testing Strategy"
date: 2025-04-26
---

# Implementation Plan P2 - Testing Strategy

## Overview

This document outlines the testing strategy required to validate the Phase 2 (P2) refactoring changes documented in `doc/changes/2025-04-26-EnzoChanges.md`. The key goals of the testing are to:

1.  Verify the correct functioning of the refactored `NIC` with dual engines (`SocketEngine`, `SharedMemoryEngine`).
2.  Confirm the POSIX shared memory implementation (`SharedMemoryEngine`) enables reliable intra-vehicle communication.
3.  Ensure the `SocketEngine` continues to function correctly for inter-vehicle communication via the new engine interface.
4.  Validate the `NIC`'s routing logic (selecting internal vs. external engine based on destination MAC).
5.  Verify the correct implementation of message origin tracking (`Message::_origin` field).
6.  Ensure the integration of these changes (`Traits`, `Vehicle`, `Initializer`, Type Aliases) is correct.

## Testing Strategy

The strategy involves adapting existing unit and integration tests to reflect the new architecture and interfaces, and creating new integration tests specifically targeting the P2 communication scenarios.

-   **Unit Tests:** Focus on testing individual components (especially the refactored engines and `Message`) in isolation.
-   **Integration Tests:** Focus on testing the interactions between components of the communication stack (`Communicator`, `Protocol`, `NIC`, `Engines`) and end-to-end communication scenarios within and between vehicles.
-   **System Tests:** Adapt existing system tests (`demo.cpp`, `virtual_dst_address_test.cpp`) if necessary, although the primary focus will be on integration tests for verifying the core P2 communication logic.

## Existing Test File Adaptations

Based on a scan of the `tests/` subdirectories, the following adaptations are needed:

1.  **`tests/unit_tests/sharedMemoryEngine_test.cpp`:**
    *   **Action:** Requires **significant rewriting**. The previous tests likely targeted the old static map/mutex/eventfd implementation.
    *   **New Focus:**
        *   Test the POSIX shared memory setup (`shm_open`, `mmap`, `ftruncate`) and semaphore creation (`sem_open`).
        *   Test initialization (`initializeSharedMemory`) vs. attachment (`attachToSharedMemory`) logic, including the wait for the `initialized` flag.
        *   Test `send()` and `receiveData()` logic, verifying correct data transfer (payload and protocol number) through the shared ring buffer.
        *   **Crucially:** Test concurrent access from multiple threads simulating different components sending/receiving simultaneously to validate the semaphore synchronization (`_mutex_sem`, `_items_sem`, `_space_sem`).
        *   Test the `ref_count` mechanism and resource cleanup (`cleanupSharedResources`), including `sem_unlink` and `shm_unlink` on last detachment.
        *   Verify `getNotificationFd()` returns the timerfd.
        *   Verify `getMacAddress()` returns `Ethernet::NULL_ADDRESS`.

2.  **`tests/unit_tests/socketEngine_test.cpp`:**
    *   **Action:** Needs **adaptation**.
    *   **New Focus:**
        *   Remove tests related to the internal event loop (`run`, `epoll`, `stop_ev`).
        *   Adapt tests for `start()` and `stop()` to reflect their simplified logic (socket setup/close).
        *   Add tests for the new `receiveFrame()` method. Simulate data arriving on the socket (`_sock_fd`) and call `receiveFrame` to verify correct data reception and protocol byte order conversion (`ntohs`). Test return values for data received, no data (EAGAIN), and errors.
        *   Add tests for `getMacAddress()` and `getNotificationFd()` to ensure they return the correct values.
        *   Keep/adapt tests for `send()`, ensuring it still functions correctly.

3.  **`tests/unit_tests/message_test.cpp`:**
    *   **Action:** Needs **adaptation**.
    *   **New Focus:**
        *   Verify the `_data` member functions correctly with `unsigned char[]` instead of `void*[]`.
        *   Add tests for the `origin()` getter and `origin(const TheAddress&)` setter.
        *   Ensure constructors and assignment operator correctly handle the `_origin` field.

4.  **`tests/integration_tests/nic_test.cpp`:**
    *   **Action:** Requires **significant adaptation or rewriting**.
    *   **New Focus:**
        *   Instantiate the `NIC` with concrete `SocketEngine` and `SharedMemoryEngine` (or potentially mock engines if feasible).
        *   Test `send()` routing: Send buffers with `dst_mac == nic.address()` and verify they likely go through `SharedMemoryEngine` (e.g., by checking statistics or mocking `send` on the engines). Send buffers with `dst_mac != nic.address()` and verify they go through `SocketEngine`.
        *   Test `receive()` path: Simulate data arrival via `SocketEngine::receiveFrame` and verify `Protocol` gets notified. Simulate data arrival via `SharedMemoryEngine::receiveData` and verify `Protocol` gets notified with a correctly reconstructed frame.
        *   Verify buffer allocation (`alloc`) and freeing (`free`).
        *   Verify statistics (`statistics()`) counters for both internal and external paths.

5.  **`tests/integration_tests/communicator_test.cpp`:**
    *   **Action:** Needs **adaptation**.
    *   **New Focus:**
        *   Update test setup to use the fully refactored stack (`TheCommunicator`, `TheProtocol`, `TheNIC`, `SocketEngine`, `SharedMemoryEngine`).
        *   Create test cases for **intra-vehicle** communication: Set up one `Vehicle`, two `Components` (e.g., Sender, Receiver), have them `send`/`receive`, verify data integrity, and **crucially verify the `message.origin()` is correct** on the receiver side.
        *   Create test cases for **inter-vehicle** communication: Set up two `Vehicle` instances (requires careful handling of processes or advanced setup within one test process if using the dummy interface), create components in each, have them `send`/`receive`, verify data integrity, and **verify `message.origin()` is correct**.

6.  **`tests/integration_tests/protocol_test.cpp`:**
    *   **Action:** Needs **adaptation**.
    *   **New Focus:**
        *   Update test setup to use the fully refactored stack (`TheProtocol`, `TheNIC`, etc.).
        *   Ensure tests for sending/receiving data packets with headers still pass.
        *   Verify correct attachment/detachment of observers based on port numbers.

7.  **`tests/integration_tests/vehicle_test.cpp` & `initializer_test.cpp`:**
    *   **Action:** Minor review and potential adaptation.
    *   **New Focus:** Primarily ensure they compile and run correctly with the updated type aliases (`TheNIC`, `TheProtocol`). Verify that `Initializer::create_vehicle` successfully creates the dual-engine `NIC`. The core logic testing vehicle/component lifecycle should remain valid.

8.  **`tests/system_tests/`:**
    *   **Action:** Review and adapt as needed.
    *   **New Focus:** Update `demo.cpp` and `virtual_dst_address_test.cpp` to use the new stack setup via `Initializer`. Ensure they demonstrate the intended high-level behavior correctly.

## New Test Files

Consider creating a dedicated integration test file for clarity:

1.  **`tests/integration_tests/p2_communication_test.cpp`:**
    *   **Purpose:** Consolidate the core P2 communication validation.
    *   **Contents:**
        *   `test_intra_vehicle_comm()`: Creates one `Vehicle`, two `Components` (e.g., `SimpleSenderComponent`, `SimpleReceiverComponent`), sends messages between them, verifies reception, data integrity, and correct `message.origin()`. This implicitly tests the `SharedMemoryEngine` path and `NIC` routing for local destinations.
        *   `test_inter_vehicle_comm()`: Creates two `Vehicle` instances, components in each, sends messages between them using the dummy interface, verifies reception, data integrity, and correct `message.origin()`. This implicitly tests the `SocketEngine` path and `NIC` routing for external destinations.
        *   (Optional) `test_nic_multiplex_reception()`: Attempt to send messages via both internal and external paths concurrently (or near-concurrently) and verify the `NIC` correctly receives and forwards both to the `Protocol` layer.

## Makefile Adaptations

1.  **New Test Files:** If new files like `p2_communication_test.cpp` are added to `tests/integration_tests/`, they need to be added to the `INTEGRATION_TEST_SRCS` variable (or detected by the wildcard) and the corresponding binary path added to `INTEGRATION_TEST_BINS`. The existing compilation rules should handle building them.
    ```makefile
    # Example addition (if not covered by wildcard)
    INTEGRATION_TEST_SRCS := $(wildcard $(INTEGRATION_TESTDIR)/*.cpp) tests/integration_tests/p2_communication_test.cpp
    INTEGRATION_TEST_BINS := $(patsubst $(INTEGRATION_TESTDIR)/%.cpp, $(BINDIR)/integration_tests/%, $(INTEGRATION_TEST_SRCS))
    ```
2.  **Linker Flags:** The use of POSIX shared memory (`shm_open`, `shm_unlink`) and named semaphores (`sem_open`, `sem_close`, `sem_unlink`) requires linking against the real-time library. Add `-lrt` to `LDFLAGS`:
    ```makefile
    LDFLAGS = -pthread -lrt
    ```
3.  **Dependencies:** The Makefile relies on implicit rules and wildcard source detection. This should generally work for the refactored files, as long as all necessary `.cpp` files are included in `SRCS` (if they exist and are not header-only). Ensure any new `.cpp` implementation files are placed where the wildcard will find them or are explicitly added.

## Running Tests

-   Use `make test` to compile and run all tests.
-   Use `make run_unit_tests`, `make run_integration_tests`, `make run_system_tests` for specific groups.
-   Use target patterns like `make run_integration_p2_communication_test` to run specific new tests.

This plan provides a comprehensive approach to validate the P2 refactoring. Implementing these test adaptations and additions is crucial before considering the P2 work complete. 