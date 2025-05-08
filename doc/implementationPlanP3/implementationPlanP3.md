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
    *   The producer will detect and process `INTEREST` messages for its `_produced_data_type` directly in the `component_dispatcher_routine`, which will update the list of requested periods and recalculate the GCD.

3.  **Consumer Behavior:**
    *   A consumer component can be interested in multiple `DataTypeId`s, each with a specific desired period.
    *   Consumers will send `INTEREST` messages on startup. Interest lifetime is indefinite for P3.
    *   **Consumer-Side Filtering:** To manage data arrival, consumers will use their own local clock. If a `RESPONSE` message for a given `DataTypeId` arrives *sooner* than the consumer's requested `period` for that type (since the last accepted message of that type), the message will be discarded at a low level within the communication stack (specifically, by the `Communicator`) to avoid unnecessary processing by the component's application logic.

4.  **Interest Message Routing (via Gateway with Targeted Internal Relay):**
    *   Producer components will **register** with the `GatewayComponent` upon startup, declaring the `DataTypeId` they produce.
    *   The `GatewayComponent` will maintain a registry mapping `DataTypeId`s to the internal `Protocol::Port`s of the components that produce them.
    *   Consumer components will send `INTEREST` messages as a logical broadcast to a fixed port, **Port 0**, targeting the `GatewayComponent` (physical destination: Ethernet broadcast).
    *   The `GatewayComponent` (listening on Port 0 of its vehicle's MAC address) will receive these *external* `INTEREST` messages.
    *   The `GatewayComponent` will override the `component_dispatcher_routine` method to directly process incoming `REG_PRODUCER` and `INTEREST` messages without relying on typed handlers.
    *   Upon receiving an external `INTEREST` for a `DataTypeId X`:
        *   The `GatewayComponent` will consult its registry.
        *   For each registered internal `Protocol::Port` of a producer for `DataTypeId X`, the Gateway will send a *new, targeted* `INTEREST` message (logical unicast) directly to that `ProducerComponent`'s `Port` (on the Gateway's own vehicle MAC address).
    *   This targeted relay ensures only relevant producer components are notified, optimizing internal message traffic.

5.  **API-Managed Concurrency for Consumers:**
    *   To simplify component application logic, the communication library (specifically the `Component` base class and helper classes) will manage threads for handling incoming data for each registered interest.
    *   Components will register interest in specific `DataTypeId`s by providing a callback function.
    *   **Component Dispatcher Thread:** Each component instance will have a single "dispatcher" thread (`_component_dispatcher_thread`). Its primary role is to:
        1.  Call the component's `_communicator->receive()` method, which blocks until a message arrives. This message has *already* been pre-filtered by the `Communicator` for consumer-side periodicity (if applicable to the component's role and message type).
        2.  Once a valid message is received, the dispatcher thread creates a heap-allocated copy of this message.
        3.  It then notifies an internal `Conditionally_Data_Observed<Message, DataTypeId>` object (`_internal_typed_observed`) within the component, using the message's `DataTypeId` as the notification condition and passing the heap-allocated message.
    *   **TypedDataHandler Threads:** For each call to `register_interest_handler(type, period, callback)`, a `TypedDataHandler` instance is created.
        1.  This handler is a `Concurrent_Observer<Message, DataTypeId>` and registers itself with the component's `_internal_typed_observed` for the specific `type`.
        2.  Crucially, each `TypedDataHandler` runs its own dedicated thread (`processing_loop`). This thread blocks on its internal semaphore, waiting for its `update()` method to be called (which happens when `_internal_typed_observed` notifies it).
        3.  When awakened, it retrieves the heap-allocated `Message` and executes the user-supplied `callback` function with this message.
        4.  This model allows multiple data types to be processed concurrently by the consumer component, with each type handled by a dedicated thread, all managed by the library. The component developer only needs to provide the callback logic.

## 2.6. Justification of Filtering Placement

The placement of message filtering mechanisms is critical for balancing efficiency, complexity, and adherence to software layering principles.

1.  **`DataTypeId` Filtering (in `Communicator::update`):**
    *   **Goal:** Discard messages with `DataTypeId`s irrelevant to the receiving component as early as possible.
    *   **Challenge:** To access `DataTypeId`, a portion of the message payload (specifically, the `Message` header) must be deserialized.
    *   **Considered Alternatives & Reasoning:**
        *   **`NIC` Layer:** The `NIC` operates at the Ethernet frame level. Requiring it to parse `Message` headers would deeply violate layering principles, coupling it tightly to higher-level application-specific formats. This is not a viable option.
        *   **`Protocol` Layer:** The `Protocol` layer handles its own packet structure (e.g., demultiplexing by port). It *could* be modified to also parse the `Message` header from its payload to extract the `DataTypeId`. However, this would:
            *   Increase the complexity of the `Protocol` layer, as it would need to understand `Message` structure.
            *   Require the `Observer` mechanism between `Protocol` and `Communicator` to use a more complex condition (e.g., `std::pair<Port, DataTypeId>`).
            *   Introduce special handling for the `GatewayComponent`, which needs to receive `INTEREST` messages of potentially any `DataTypeId` on its designated port (Port 0). This would necessitate wildcard `DataTypeId` registrations or specific logic in `Protocol`.
        *   **`Communicator` Layer (Current Plan):** The `Communicator` is instantiated per component. When its `update()` method (called by `Protocol` upon message arrival for its port) receives a buffer, it deserializes the `Message` header. This is component-specific processing.
            *   **Rationale:** This placement keeps the `Protocol` layer clean and focused on port-based demultiplexing. The `Communicator` is the first point where component-specific logic (like caring about specific `DataTypeId`s) is naturally applied. While the deserialization happens here, the message has already been routed to the correct component's `Communicator` by the `Protocol`. The overhead of this header deserialization in `Communicator::update` is generally acceptable, and the architectural clarity and simplicity of `Protocol` are maintained. The benefit of moving this specific deserialization one step further down to `Protocol` is likely outweighed by the increased complexity introduced in `Protocol`.

2.  **Consumer-Side Period Filtering (in `Communicator::update`):**
    *   **Goal:** Allow a consumer component to discard `RESPONSE` messages that arrive more frequently than its requested period for a given `DataTypeId`, preventing unnecessary processing by the component's application logic.
    *   **Challenge:** This filtering is inherently stateful and specific to each consumer component's interest. It requires knowing:
        1.  The `period_us` requested by *this specific component* for *this specific `DataTypeId`*.
        2.  The `last_accepted_response_time_us` for that `DataTypeId` by *this specific component*.
    *   **Considered Alternatives & Reasoning:**
        *   **Lower Layers (`Protocol`, `NIC`):** These layers are designed to be stateless regarding individual component application logic or specific interest details. They are shared resources within a vehicle (Protocol) or system (NIC).
            *   Implementing component-specific, stateful period filtering here would require these lower layers to query or access per-component, per-interest state. This would severely break layering, introduce tight coupling, and create significant complexity in state management and synchronization.
        *   **`Communicator` Layer (Current Plan):** The `Communicator` is already tied to a single `Component` instance (via `_owner_component`). It can directly and efficiently access the `_active_interests` list (containing `period_us` and `last_accepted_response_time_us`) of its owning component.
            *   **Rationale:** This is the lowest practical and architecturally sound level for such component-specific, stateful filtering. The check (getting current time, accessing component state, and comparing) is computationally inexpensive. Performing it in `Communicator::update` effectively prevents the message from being queued for the component's dispatcher thread and subsequent `TypedDataHandler` if it's too early, thus saving processing resources without compromising architectural principles.

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
        SYSTEM_INTERNAL_REG_PRODUCER // Potentially, or use a new Message::Type
    };
    ```
*   **Update `Message::Type` enum (in `message.h`):**
    ```cpp
    enum class Type : std::uint8_t {
        INTEREST,
        RESPONSE,
        REG_PRODUCER, // New type for producer registration with Gateway
        // PTP,
        // JOIN,
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
        *   **Updated Component Dispatcher Routine:**
            The `component_dispatcher_routine` will now check for `INTEREST` messages that match a producer's `_produced_data_type` and handle them directly by updating `_received_interest_periods` and calling `update_gcd_period()`.
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
        *   For producers: Check if this is an `INTEREST` message for this producer's `_produced_data_type`:
            ```cpp
            if (_produced_data_type != DataTypeId::UNKNOWN && 
                message.message_type() == Message::Type::INTEREST &&
                message.unit_type() == _produced_data_type) {
                
                // Add this period to our received_interest_periods if not already present
                _received_interest_periods.push_back(message.period());
                update_gcd_period();
            }
            ```
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
    *   **Add Registration on Startup (e.g., in `Component::start()` or early in `run()` for producers):**
        ```cpp
        // Inside producer's startup sequence
        if (/* is a producer component */) {
            Message reg_msg = _communicator->new_message(
                Message::Type::REG_PRODUCER,
                static_cast<std::uint32_t>(this->_produced_data_type) // unit_type carries the DataTypeId being produced
            );
            // Send to Gateway's well-known port (0) on the local vehicle.
            // The physical address part of Protocol::Address for Gateway can be vehicle's own MAC or Ethernet Broadcast.
            // For intra-vehicle, vehicle's own MAC is more precise.
            _communicator->send(reg_msg, Protocol::Address(this->vehicle()->address().paddr(), 0));
            db<Component>(INF) << getName() << " sent REG_PRODUCER for type " << (int)this->_produced_data_type << " to Gateway.
";
        }
        ```
    *   `run()` (or similar, called by `_component_dispatcher_thread` if also consuming, or a dedicated listener for *relayed* INTERESTs):
        *   When a *relayed* `INTEREST` for `this->_produced_data_type` is received via its `_communicator` (from Gateway):
            *   Add `msg.period()` to `_received_interest_periods`.
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

*   **Add Internal Registry:**
    *   `std::map<DataTypeId, std::vector<Protocol::Port>> _producer_registry;`
*   **Constructor:**
    *   Ensure its `_communicator` is initialized to listen on Port 0.
        `_communicator = new Comms(_vehicle->protocol(), VehicleProt::Address(_vehicle->address().paddr(), 0));`
*   **Override `component_dispatcher_routine`:**
    * The Gateway will override the base `component_dispatcher_routine` to directly process incoming messages based on their type, rather than relying on typed handlers.
    * This ensures the Gateway can handle all `REG_PRODUCER` and `INTEREST` messages without needing to register handlers for each possible `DataTypeId`.
    ```cpp
    void GatewayComponent::component_dispatcher_routine() override {
        // Receive messages
        while (_dispatcher_running.load()) {
            // ... receive message ...
            
            // Process based on message type
            if (message.message_type() == Message::Type::REG_PRODUCER) {
                handle_reg_producer(message);
            } 
            else if (message.message_type() == Message::Type::INTEREST) {
                handle_interest(message);
            }
        }
    }
    ```
*   **Handle Producer Registration:**
    *   If an incoming `Message reg_msg` is of `Message::Type::REG_PRODUCER`:
        *   `DataTypeId produced_type = static_cast<DataTypeId>(reg_msg.unit_type());`
        *   `Protocol::Port producer_port = reg_msg.origin().port();`
        *   `_producer_registry[produced_type].push_back(producer_port);`
        *   `// Optional: Add logic to prevent duplicate port entries for the same type.`
        *   `db<GatewayComponent>(INF) << "Registered producer for type " << (int)produced_type << " on port " << producer_port << " from origin " << reg_msg.origin().to_string() <<".
";`
*   **Handle External `INTEREST` and Relay Targetedly:**
    *   If an incoming `Message external_interest_msg` is of `Message::Type::INTEREST`:
        *   `DataTypeId requested_type = static_cast<DataTypeId>(external_interest_msg.unit_type());`
        *   `unsigned int period = external_interest_msg.period();`
        *   `db<GatewayComponent>(INF) << "Gateway received EXTERNAL INTEREST for type " << (int)requested_type << " from " << external_interest_msg.origin().to_string() << ".
";`
        *   `if (_producer_registry.count(requested_type)) {`
            *   `for (Protocol::Port target_producer_port : _producer_registry[requested_type]) {`
                *   `Message internal_interest = _communicator->new_message(Message::Type::INTEREST, static_cast<std::uint32_t>(requested_type), period);`
                *   `// Target is specific producer port on own vehicle's MAC`
                *   `_communicator->send(internal_interest, Protocol::Address(_vehicle->address().paddr(), target_producer_port));`
                *   `db<GatewayComponent>(INF) << "Gateway relayed INTEREST for type " << (int)requested_type << " to internal port " << target_producer_port << ".
";`
            *   `}`
        *   `} else {`
            *   `db<GatewayComponent>(WRN) << "Gateway: No local producer registered for type " << (int)requested_type << ".
";`
        *   `}`

### 3.5. `protocol.h`

*   **`Protocol::update()`:**
    *   The existing logic for distinguishing internal broadcast (`src_mac == my_mac && dst_port == 0`) from other messages is suitable.
    *   The `_observed.notifyAll(buf)` for internal broadcasts will **not** be the primary path for Gateway-relayed `INTEREST` messages anymore. Instead, the Gateway will perform targeted sends to specific internal `<MAC_SELF>:<PRODUCER_PORT>`, which will use the `_observed.notify(target_producer_port, buf)` path in `Protocol::update`. The `notifyAll` path remains useful for other genuine internal broadcast scenarios if any.
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

*   **Origin preservation in Gateway:** Clarified: When Gateway relays an INTEREST, the new internal INTEREST will have Gateway's origin. This is the accepted approach as P3 responses are broadcast, so direct reply to the original external requester is not a primary concern for the producer component itself.
*   **Producer's response to changing GCD:** Clarified: The producer's response thread should adapt immediately if the GCD period changes (e.g., a new interest arrives modifying the GCD). This might involve signaling the existing thread with the new period or, if simpler to implement robustly, stopping and restarting the response thread with the updated GCD. Consumer-side filtering will handle any transient extra messages.
*   **Stopping `TypedDataHandler` threads:** Ensure `Component::stop()` correctly signals and joins these handler threads. Posting to their semaphore is key to unblock them.
*   **Memory Management of `Message` objects:** The `Component` dispatcher creates `new Message()` for `_internal_typed_observed.notify()`. The `TypedDataHandler` that receives it via its `updated()` method is responsible for `delete msg;`. This must be consistent.

This plan provides a detailed roadmap. Each step will involve careful implementation and unit/integration testing. 