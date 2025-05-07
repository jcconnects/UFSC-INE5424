# P3 Implementation Plan: Time-Triggered Publish-Subscribe

This document outlines the implementation plan for Project Stage 3 (P3), focusing on time-triggered publish-subscribe communication between components and systems. It incorporates decisions made regarding network efficiency, component responsibilities, and internal API structure for managing concurrency.

## 1. Core P3 Requirements

- Communication agents (components or systems) interact via **Interest** and **Response** messages.
- **Interest Messages (`I`):** `{origin, type, period}`
    - `origin`: Full `Protocol::Address` of the requester.
    *   `type`: A `DataTypeId` (TEDS-like code) identifying the requested data.
    *   `period`: Requested update interval in microseconds.
- **Response Messages (`R`):** `{origin, type, value}`
    - `origin`: Full `Protocol::Address` of the data producer.
    - `type`: The `DataTypeId` of the data being provided.
    - `value`: The actual data payload (byte array).
- All messages (Interests and Responses) are sent via **logical broadcast**.
- Producers send Responses **periodically** based on received Interests.
- Temporal synchronization of machines is assumed.
- **Efficiency Goals:**
    - Minimize network occupation and message latency.
    - System must handle thousands of vehicles and components.

## 2. Key Design Decisions & Optimizations

1.  **`DataTypeId` for Message Typing:**
    *   An `enum class DataTypeId : std::uint32_t` will be defined (e.g., in `message.h` or `teds.h`) to uniquely identify all data types in the system (e.g., `VEHICLE_SPEED`, `LIDAR_SCAN`). This ID will be used in `Message::_unit_type`.

2.  **Producer Behavior:**
    *   Each producer component generates only **one** `DataTypeId`.
    *   When a producer receives multiple `INTEREST` messages for its `DataTypeId` with different periods, it will calculate the **Greatest Common Divisor (GCD)** of all active requested periods.
    *   The producer will then send `RESPONSE` messages (logical broadcast) at this GCD period.
    *   A dedicated thread within the producer component will manage this periodic sending.

3.  **Consumer Behavior:**
    *   A consumer component can be interested in multiple `DataTypeId`s, each with a specific desired period.
    *   Consumers will send `INTEREST` messages on startup. Interest lifetime is indefinite for P3.
    *   **Consumer-Side Filtering:** To manage data arrival, consumers will use their own local clock. If a `RESPONSE` message for a given `DataTypeId` arrives *sooner* than the consumer's requested `period` for that type (since the last accepted message of that type), the message will be discarded at a low level within the communication stack (specifically, by the `Communicator`) to avoid unnecessary processing by the component's application logic.

4.  **Interest Message Routing (via Gateway):**
    *   Consumer components will send `INTEREST` messages as a logical broadcast to a fixed port, **Port 0**, which is designated for the `GatewayComponent`. The physical destination will be the Ethernet broadcast address.
    *   The `GatewayComponent` (listening on Port 0 of its vehicle's MAC address) will receive these `INTEREST` messages.
    *   Upon receiving an `INTEREST`, the `GatewayComponent` will perform an **internal logical broadcast** of this `INTEREST` message. This internal broadcast will use the vehicle's own MAC address as the source and destination MAC, and Port 0 as the destination port.
    *   Producer components within the same vehicle, which are observing the protocol, will receive this internally broadcasted `INTEREST`.

5.  **API-Managed Concurrency for Consumers:**
    *   To simplify component application logic, the communication library (specifically the `Component` base class and helper classes) will manage threads for handling incoming data for each registered interest.
    *   Components will register interest in specific `DataTypeId`s by providing a callback function. A dedicated thread will be managed by the library for each registered interest, invoking the callback when valid data arrives.

## 3. Detailed Changes by Module

### 3.1. `message.h` / `teds.h`

*   **Define `enum class DataTypeId : std::uint32_t`:**
    ```cpp
    // e.g., in a new include/teds.h or within message.h
    enum class DataTypeId : std::uint32_t {
        UNKNOWN = 0,
        VEHICLE_SPEED,
        ENGINE_RPM,
        OBSTACLE_DISTANCE,
        // ... other necessary data types
    };
    ```
*   **`Message` Class:**
    *   Ensure `_unit_type` (uses `DataTypeId`) and `_period` (for `INTEREST`) are correctly handled in constructors, serialization, and deserialization.
    *   The `Message::Origin` (which is `Protocol::Address`) will capture the full source address.
    *   `_timestamp` is set at message creation.

### 3.2. `component.h` (Base Class and Derived Components)

*   **`Component` Base Class Additions:**
    *   **For All Components:**
        *   `std::vector<std::unique_ptr<class TypedDataHandler>> _typed_data_handlers;`
        *   `std::vector<pthread_t> _handler_threads;`
        *   `Conditionally_Data_Observed<Message, DataTypeId> _internal_typed_observed;`
        *   `pthread_t _component_dispatcher_thread;`
        *   `void component_dispatcher_routine();` (static, takes `this` as arg)
        *   Protected method:
            `void register_interest_handler(DataTypeId type, std::uint32_t period_us, std::function<void(const Message&)> callback);`
    *   **Specifically for Consumers (managed by `register_interest_handler`):**
        *   `struct InterestRequest { DataTypeId type; std::uint32_t period_us; std::uint64_t last_accepted_response_time_us; bool interest_sent; };`
        *   `std::vector<InterestRequest> _active_interests;` (This list will be accessible by the `Communicator` for filtering).
    *   **Specifically for Producers:**
        *   `const DataTypeId _produced_data_type;` (Initialized in derived producer constructor).
        *   `std::vector<std::uint32_t> _received_interest_periods;`
        *   `std::atomic<std::uint32_t> _current_gcd_period_us;`
        *   `pthread_t _producer_response_thread;`
        *   `void producer_response_routine();` (static, takes `this` as arg)
        *   `void start_producer_thread();`
        *   `void stop_producer_thread();`
        *   Helper: `std::uint32_t calculate_gcd(std::uint32_t a, std::uint32_t b);` and `void update_gcd_period();`

*   **`Component::start()`:**
    *   Launch `_component_dispatcher_thread`.
    *   If it's a producer, call `start_producer_thread()`.

*   **`Component::stop()`:**
    *   Signal and join `_component_dispatcher_thread`.
    *   Signal and join all `_handler_threads` (for consumers).
    *   If it's a producer, call `stop_producer_thread()`.
    *   Ensure `_communicator->close()` is called to unblock its receive.

*   **`Component::component_dispatcher_routine()` (Consumer focus):**
    *   Loop while `_running`.
    *   `Message raw_msg;`
    *   `if (_communicator->receive(&raw_msg)) { // This receive is now filtered by Communicator`
        *   `Message* heap_msg = new Message(raw_msg);`
        *   `if (!_internal_typed_observed.notify(heap_msg->unit_type(), heap_msg)) { delete heap_msg; }`
    *   `}`

*   **`Component::register_interest_handler(...)` (Consumer):**
    1.  Adds an `InterestRequest` entry to `_active_interests`.
    2.  Creates a `TypedDataHandler` (see section 3.8) for the given `DataTypeId` and callback.
    3.  Attaches this handler to `_internal_typed_observed` using `DataTypeId` as the condition.
    4.  Launches the `TypedDataHandler`'s processing loop in a new thread (store handle in `_handler_threads`).
    5.  Sends the initial `INTEREST` message:
        *   `Message interest_msg = _communicator->new_message(Message::Type::INTEREST, type, period_us);`
        *   `_communicator->send(interest_msg, Protocol::Address(Ethernet::BROADCAST, 0)); // To Gateway`

*   **Producer Derived Component (e.g., `LidarComponent`):**
    *   Constructor: Initialize `_produced_data_type`.
    *   `run()` (or similar, called by `_component_dispatcher_thread` if also consuming, or a dedicated listener):
        *   If it needs to *receive* messages (e.g. INTERESTs if not purely handled by Gateway's broadcast):
            *   When an `INTEREST` for `this->_produced_data_type` is received via its `_communicator` (after Gateway broadcast):
                *   Add `msg.period()` to `_received_interest_periods`.
                *   Call `update_gcd_period()`.
                *   If `_producer_response_thread` is not active or period changed, (re)start it or signal it.
    *   `producer_response_routine()`:
        *   Loop while `_running` and `_current_gcd_period_us > 0`.
        *   `current_data = produce_data_for(_produced_data_type);` (Actual data generation).
        *   Serialize `current_data`.
        *   `Message response_msg = _communicator->new_message(Message::Type::RESPONSE, _produced_data_type, 0, serialized_data.data(), serialized_data.size());`
        *   `_communicator->send(response_msg, Protocol::Address::BROADCAST);`
        *   `usleep(_current_gcd_period_us.load());`

### 3.3. `communicator.h`

*   Add `Component* _owner_component;` (passed in constructor).
*   **`Communicator::update(typename Channel::Observed* obs, typename Channel::Observer::Observing_Condition c, Buffer* buf)`:**
    *   This is called by `Protocol` when a new frame arrives for the Communicator's port.
    *   `if (!buf) { Observer::update(c, nullptr); return; } // Handle close signal`
    *   Deserialize message from `buf` into a local `Message msg_temp;` (Need to extract payload correctly from `Ethernet::Frame` within `Buffer`).
    *   **If `_owner_component` is a consumer (check via a dynamic_cast or a role flag in Component):**
        *   If `msg_temp.message_type() == Message::Type::RESPONSE`:
            *   Iterate `_owner_component->_active_interests`.
            *   If `req.type == msg_temp.unit_type()`:
                *   `current_time_us = /* get current time in microseconds */;`
                *   `if (current_time_us - req.last_accepted_response_time_us >= req.period_us)`:
                    *   `req.last_accepted_response_time_us = current_time_us;`
                    *   `Observer::update(c, buf); // Pass original buffer to unblock receive()`
                    *   `return; // Processed`
                *   Else (too early), `db<Communicator>(INF) << "Discarding early RESPONSE for type " << msg_temp.unit_type() << "
";`
    *   **If `_owner_component` is a producer (or Gateway receiving an Interest):**
        *   If `msg_temp.message_type() == Message::Type::INTEREST`:
            *   (For producers specifically) If `msg_temp.unit_type() == _owner_component->_produced_data_type`:
                 *   `Observer::update(c, buf); // Pass buffer to unblock receive()`
                 *   `return;`
    *   `_channel->free(buf); // If not processed by any rule above`

*   **`Communicator::receive(Message* message)`:**
    *   The core logic `Buffer* buf = Observer::updated();` remains.
    *   If `buf` is not null, deserialize its content (which is now pre-filtered by `Communicator::update`) into `*message`.
    *   `_channel->free(buf);` after processing.

### 3.4. `GatewayComponent.h` (New or existing, derived from `Component`)

*   Constructor:
    *   Ensure its `_communicator` is initialized to listen on Port 0.
        `_communicator = new Comms(_vehicle->protocol(), VehicleProt::Address(_vehicle->address().paddr(), 0));`
*   `run()` or equivalent logic in `component_dispatcher_routine`:
    *   Receives messages via its `_communicator`.
    *   If an incoming `Message interest_msg` is of `Message::Type::INTEREST`:
        *   Log reception.
        *   Create a new `INTEREST` message for internal broadcast:
            `Message internal_interest = _communicator->new_message(Message::Type::INTEREST, interest_msg.unit_type(), interest_msg.period());`
            *(Communicator's `new_message` uses its own address as origin, which is Gateway's here).*
        *   Send this internal interest:
            `_communicator->send(internal_interest, VehicleProt::Address(_vehicle->address().paddr(), 0));`
            *(This MAC:0 destination will be treated as an internal broadcast by `Protocol`)*.

### 3.5. `protocol.h`

*   **`Protocol::update()`:**
    *   The existing logic for distinguishing internal broadcast (`src_mac == my_mac && dst_port == 0`) from other messages is suitable.
    *   `_observed.notifyAll(buf)` for internal broadcasts will correctly notify all `Communicator`s (including producers that need to see the Gateway-relayed INTEREST).
    *   `_observed.notify(dst_port, buf)` for specific ports remains.
*   No direct changes for `DataTypeId` filtering are needed here if `Communicator` handles it.

### 3.6. `nic.h`, `socketEngine.h`, `sharedMemoryEngine.h`

*   No direct changes anticipated for P3 logic related to Interest/Response or `DataTypeId`.
*   **Linter Errors:** Must be addressed by configuring include paths correctly in the development environment (e.g., for `semaphore.h`, `pthread.h`, `unistd.h`).

### 3.7. `observed.h`, `observer.h`

*   **`Conditionally_Data_Observed<Message, DataTypeId>`:** Will be used inside `Component` for typed dispatch to handlers.
*   **`Concurrent_Observer<Message, DataTypeId>`:** This will be the base for `TypedDataHandler`.
*   The existing `Conditional_Data_Observer<Buffer, Port>` used by `Communicator` to observe `Protocol` remains.

### 3.8. `TypedDataHandler.h` (New File)

```cpp
#ifndef TYPED_DATA_HANDLER_H
#define TYPED_DATA_HANDLER_H

#include "observer.h" // For Concurrent_Observer
#include "message.h"
#include "teds.h"     // For DataTypeId
#include "component.h" // Forward declare or include carefully
#include <functional>
#include <pthread.h>

class Component; // Forward declaration

class TypedDataHandler : public Concurrent_Observer<Message, DataTypeId> {
public:
    TypedDataHandler(DataTypeId type, 
                     std::function<void(const Message&)> callback_func, 
                     Component* parent_component);
    ~TypedDataHandler();

    void start_processing_thread();
    void stop_processing_thread(); // Signals the loop to end
    pthread_t get_thread_id() const;

private:
    static void* processing_loop_entry(void* arg);
    void processing_loop();

    std::function<void(const Message&)> _callback_func;
    Component* _parent_component;
    std::atomic<bool> _handler_running;
    pthread_t _thread_id;
};

#endif // TYPED_DATA_HANDLER_H
```
*   **Implementation (`TypedDataHandler.cpp`):**
    *   Constructor initializes members, sets `_handler_running` to false.
    *   `start_processing_thread()`: sets `_handler_running` to true, creates `_thread_id` running `processing_loop_entry`.
    *   `stop_processing_thread()`: sets `_handler_running` to false, posts to its own semaphore (`_semaphore.post()`) to unblock `updated()` if waiting, allowing the loop to exit. Caller will `pthread_join`.
    *   `processing_loop()`:
        ```cpp
        // Inside TypedDataHandler::processing_loop()
        while (_handler_running.load()) {
            Message* msg = updated(); // Blocks on its semaphore
            if (!_handler_running.load()) { // Check again after waking up
                if(msg) delete msg;
                break;
            }
            if (msg) {
                if (_parent_component && _parent_component->running()) {
                    try {
                        _callback_func(*msg);
                    } catch (const std::exception& e) { /* Log error */ }
                }
                delete msg; // Handler deletes the heap-allocated message
            }
        }
        ```

## 4. Implementation Order & Milestones

1.  **Setup & Basic Definitions:**
    *   Resolve linter/include path issues.
    *   Define `DataTypeId` enum.
    *   Minor updates to `Message` class if needed (should be mostly fine).

2.  **Core Consumer Path (Simplified):**
    *   Modify `Component` base for `_active_interests`, `_component_dispatcher_thread`, `_internal_typed_observed`.
    *   Implement `TypedDataHandler` class.
    *   Implement `Component::register_interest_handler`.
    *   Modify `Communicator::update` and `Communicator::receive` for consumer-side filtering (period check) and passing valid messages to `Component`'s dispatcher.
    *   Create a simple Consumer derived component that registers one handler.

3.  **Core Producer Path (Simplified):**
    *   Modify `Component` base for `_produced_data_type`, `_producer_response_thread`.
    *   Implement `producer_response_routine` (initially, respond to any interest at a fixed rate, no GCD).
    *   Create a simple Producer derived component.

4.  **Initial Integration Test (No Gateway):**
    *   Consumer sends `INTEREST` (broadcast).
    *   Producer receives `INTEREST`, starts sending `RESPONSE` (broadcast).
    *   Consumer's registered handler receives and processes the `RESPONSE`.

5.  **Gateway Implementation:**
    *   Implement `GatewayComponent` to listen on Port 0.
    *   Implement logic to receive external `INTEREST` and rebroadcast it internally (`MY_MAC:0`).
    *   Update Consumer to send `INTEREST` to `Ethernet::BROADCAST:0`.

6.  **Full Integration Test (With Gateway):**
    *   Consumer sends `INTEREST` to Gateway.
    *   Gateway relays `INTEREST` internally.
    *   Producer (listening to internal broadcasts) picks up `INTEREST`.
    *   Producer sends `RESPONSE`.
    *   Consumer handler processes `RESPONSE`.

7.  **Producer GCD Logic:**
    *   Implement `_received_interest_periods` storage.
    *   Implement GCD calculation and `update_gcd_period()`.
    *   Modify `producer_response_routine` to use `_current_gcd_period_us`.
    *   Test with multiple consumers requesting the same data type at different periods.

8.  **Refinement & Robustness:**
    *   Thorough testing of thread safety, resource cleanup (`Component::stop`).
    *   Error handling and logging improvements.
    *   Performance evaluation (latency, throughput under load).

## 5. Open Questions/Considerations (During Implementation)

*   **Origin preservation in Gateway:** When Gateway relays an INTEREST, the new internal INTEREST will have Gateway's origin. Is the original external origin needed by producers? (P3 responses are broadcast, so direct reply to original is not primary).
*   **Producer's response to changing GCD:** How dynamically should the producer's response thread adapt if the GCD period changes (e.g., a new interest arrives or an old one hypothetically expires)? A signal or restarting the thread might be needed.
*   **Stopping `TypedDataHandler` threads:** Ensure `Component::stop()` correctly signals and joins these handler threads. Posting to their semaphore is key to unblock them.
*   **Memory Management of `Message` objects:** The `Component` dispatcher creates `new Message()` for `_internal_typed_observed.notify()`. The `TypedDataHandler` that receives it via its `updated()` method is responsible for `delete msg;`. This must be consistent.

This plan provides a detailed roadmap. Each step will involve careful implementation and unit/integration testing. 