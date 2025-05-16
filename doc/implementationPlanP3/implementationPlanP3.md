# P3 Implementation Plan: Time-Triggered Publish-Subscribe (Simplified)

This document outlines the **simplified** implementation plan for Project Stage 3 (P3), focusing on time-triggered publish-subscribe communication between components and systems. It incorporates decisions to reduce complexity, such as removing explicit producer registration and simplifying component interactions.

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
- All messages (Interests and Responses) are sent via **logical broadcast** to a `GatewayComponent`.
- **Gateway Role:** Relays messages received on its dedicated port (Port 0) to an internal broadcast port (Port 1).
- **Producer Components:**
    *   Produce data of a **single, predefined `DataTypeId`**.
    *   Listen for `INTEREST` messages (relayed by the Gateway) for their `DataTypeId`.
    *   Calculate the **Greatest Common Divisor (GCD)** of all active requested periods.
    *   Send `RESPONSE` messages (to the Gateway port) at this GCD period.
- **Consumer Components:**
    *   Are interested in data of a **single, predefined `DataTypeId`** at a **specific `period`**.
    *   Send `INTEREST` messages (to the Gateway port).
    *   Listen for `RESPONSE` messages (relayed by the Gateway) for their `DataTypeId`.
    *   Filter `RESPONSE` messages: discard if they arrive sooner than the requested `period`.
- Temporal synchronization of machines is assumed.
- **Efficiency Goals:**
    - Minimize network occupation and message latency.
    - System must handle thousands of vehicles and components.
- **Port Allocation:**
    - Port 0: `GatewayComponent` external listening port.
    - Port 1: Internal broadcast port used by Gateway for relaying messages.
    - Ports >= 2: Regular component ports.

## 2. Key Design Decisions & Simplifications

1.  **`DataTypeId` for Message Typing:**
    *   An `enum class DataTypeId : std::uint32_t` will be defined (e.g., in `message.h` or `teds.h`) to uniquely identify all data types.

2.  **Simplified Gateway (`GatewayComponent`):**
    *   **No Producer Registration:** The Gateway does not maintain a registry of producers.
    *   **Fixed Listening Port:** Listens for all incoming `INTEREST` and `RESPONSE` messages on a dedicated external port (Port 0).
    *   **Relay Mechanism:** Upon receiving any message on Port 0, the Gateway immediately relays it by instructing its `Protocol` layer to distribute the message to all components that are set up to observe internal broadcasts (conceptually on Port 1) on its own vehicle.
    *   **Specialized Reception:** The Gateway's `Communicator` or reception logic must be configured to accept all messages on Port 0 without `DataTypeId` or periodicity filtering.

3.  **Producer Component Behavior (Single `DataTypeId`):**
    *   Each producer component is configured to produce only **one** specific `DataTypeId`.
    *   Its `Communicator` is associated with the component's unique port (e.g., >=2) for originating messages. It is also set up to receive notifications from the `Protocol` layer for messages relayed by the Gateway to the internal broadcast mechanism (conceptually, Port 1).
    *   **Interest Processing (via internal broadcast notification):**
        *   Filters incoming messages (notified for Port 1): Processes only `INTEREST` messages matching its `_produced_data_type`. Ignores `RESPONSE` messages.
        *   When a relevant `INTEREST` is received, its `period` is used to update the list of requested periods.
        *   The **Greatest Common Divisor (GCD)** of all active requested periods is recalculated.
        *   A dedicated thread within the producer component manages periodic sending of `RESPONSE` messages at this GCD period.
    *   **Response Sending:** `RESPONSE` messages are sent to the Gateway's external port (MAC_BROADCAST:Port_0).

4.  **Consumer Component Behavior (Single `DataTypeId`, Single `Period`):**
    *   Each consumer component is configured to be interested in only **one** specific `DataTypeId` at **one** specific `period`.
    *   Its `Communicator` is associated with the component's unique port (e.g., >=2) for originating messages. It is also set up to receive notifications from the `Protocol` layer for messages relayed by the Gateway to the internal broadcast mechanism (conceptually, Port 1).
    *   **Interest Sending:** On startup (or when configured), sends an `INTEREST` message for its `DataTypeId` and `period` to the Gateway's external port (MAC_BROADCAST:Port_0).
    *   **Response Processing (via internal broadcast notification):**
        *   The component's main execution logic will call `_communicator->receive()` which blocks until a message is available (after being filtered by `Communicator::update`).
        *   Upon receiving a valid `RESPONSE` (type and `DataTypeId` match, and periodicity filter in `Communicator::update` passed), the component will directly invoke its registered data callback.
        *   Ignores `INTEREST` messages.

6.  **Message Flow Summary:**
    *   **Interest:** `Consumer` -> `Gateway (Port 0)` -> `Gateway relays to Internal Broadcast (Port 1)` -> `Producer` (filters by type).
    *   **Response:** `Producer` -> `Gateway (Port 0)` -> `Gateway relays to Internal Broadcast (Port 1)` -> `Consumer` (filters by type and period).

## 2.6. Justification of Filtering Placement

The placement of message filtering mechanisms is critical:

1.  **`DataTypeId` and Message Type Filtering (in `Communicator::update` for Consumer/Producer):**
    *   **Goal:** Ensure components only process relevant message types (`INTEREST` for Producers, `RESPONSE` for Consumers) and relevant `DataTypeId`s when they are notified of an internal broadcast (relayed message).
    *   **Placement:** This logic resides in the `Communicator::update()` method of regular (non-Gateway) components when the `Observing_Condition` indicates the message came from the internal broadcast (Port 1).
    *   **Rationale:** The `Communicator` is component-specific. After the Gateway relays a message and the `Protocol` notifies relevant observers for Port 1, each such component's `Communicator` inspects the message based on its specific role (Producer/Consumer).
        *   It first checks the `Message::Type` (e.g., a Producer ignores `RESPONSE`s).
        *   Then, it checks the `DataTypeId` against the component's configured `_produced_data_type` or `_interested_data_type`.
        *   This keeps the `Protocol` layer clean. The `GatewayComponent`'s `Communicator` will have minimal to no filtering on Port 0.

2.  **Consumer-Side Period Filtering (in `Communicator::update` for Consumer):**
    *   **Goal:** Allow a consumer to discard `RESPONSE` messages that arrive too frequently.
    *   **Placement:** Also in the consumer's `Communicator::update()` method.
    *   **Rationale:** This is component-specific stateful filtering. The `Communicator`, being tied to its owner component, can access the required `period_us` and `last_accepted_response_time_us`. This prevents unnecessary processing by the component's application logic.

3.  **Gateway Reception (Port 0):**
    *   The `GatewayComponent`'s `Communicator` (or equivalent reception logic) listening on Port 0 must be configured to accept *all* messages, regardless of `DataTypeId` or `Message::Type`, to perform its relay function.

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
        // Other types like PTP, JOIN might exist but are not focus of this P3 simplification
        // REG_PRODUCER is REMOVED
    };
    ```
*   **`Message` Class:**
    *   Ensure `_unit_type` (uses `DataTypeId`), `_period` (for `INTEREST`), and `_value` (for `RESPONSE`) are correctly handled.
    *   `_origin` (`Protocol::Address`) is crucial.
    *   `_timestamp` is set at message creation.

### 3.2. `component.h` (Base Class and Derived Components)

*   **`Component` Base Class Additions/Modifications:**
    *   **Common:**
        *   Port management: Components (except Gateway) will have their `Communicator` associated with a unique component port (e.g., Port >= 2) for originating messages. For receiving relayed broadcasts, their `Communicator` will also be registered with the `Protocol` to receive notifications for messages sent to the internal broadcast address (conceptually, Port 1).
    *   **For Consumers:**
        *   `DataTypeId _interested_data_type = DataTypeId::UNKNOWN;`
        *   `std::uint32_t _interested_period_us = 0;`
        *   `std::uint64_t _last_accepted_response_time_us = 0;`
        *   `std::function<void(const Message&)> _data_callback;`
        *   `void set_interest(DataTypeId type, std::uint32_t period_us, std::function<void(const Message&)> callback);`
        *   `void send_interest_message();`
    *   **For Producers:**
        *   `DataTypeId _produced_data_type = DataTypeId::UNKNOWN;` (Initialized in derived producer constructor).
        *   `std::vector<std::uint32_t> _received_interest_periods;`
        *   `std::mutex _periods_mutex;` // To protect _received_interest_periods
        *   `std::atomic<std::uint32_t> _current_gcd_period_us;`
        *   `pthread_t _producer_response_thread;`
        *   `std::atomic<bool> _producer_thread_running;`
        *   `sem_t _producer_semaphore;` // To signal period changes or stop
        *   `static void* producer_response_routine_entry(void* arg);`
        *   `void producer_response_routine();`
        *   `void start_producer_thread();`
        *   `void stop_producer_thread();`
        *   `void update_gcd_period();` (Calculates GCD and signals producer thread if changed)
        *   `uint32_t calculate_gcd_of_vector(const std::vector<uint32_t>& periods);`

*   **`Component::start()`:**
    *   If Consumer and `_interested_data_type != DataTypeId::UNKNOWN`, call `send_interest_message()`.
    *   If Producer and `_produced_data_type != DataTypeId::UNKNOWN`, call `start_producer_thread()`.

*   **`Component::stop()`:**
    *   If Producer, call `stop_producer_thread()`.
    *   Ensure `_communicator->close()` is called.

*   **`Component::set_interest(...)` (Consumer):**
    1.  Sets `_interested_data_type`, `_interested_period_us`, `_data_callback`.
    2.  If component is already running, call `send_interest_message()`.

*   **`Component::send_interest_message()` (Consumer):**
    *   `Message interest_msg = _communicator->new_message(Message::Type::INTEREST, _interested_data_type, _interested_period_us);`
    *   `_communicator->send(interest_msg, Protocol::Address(Ethernet::BROADCAST, 0)); // To Gateway Port 0`

*   **Producer Derived Component (e.g., `LidarComponent`):**
    *   Constructor: Initialize `_produced_data_type`. The component's `Communicator` is initialized with its unique port (e.g. >=2) and also set up to observe internal broadcasts (Port 1) from the `Protocol`.
    *   The `_communicator` for this producer will be set to listen on internal broadcast port (Port 1).
    *   `handle_received_interest(std::uint32_t period)`: (Called by its `Communicator` when a relevant relayed INTEREST is processed)
        *   `std::lock_guard<std::mutex> lock(_periods_mutex);`
        *   Add `period` to `_received_interest_periods` (avoid duplicates if necessary).
        *   Call `update_gcd_period()`.
    *   `producer_response_routine()`:
        *   Loop while `_producer_thread_running`.
        *   `current_period_us = _current_gcd_period_us.load();`
        *   If `current_period_us == 0`, wait on semaphore (or sleep and recheck).
        *   `sem_timedwait(&_producer_semaphore, &timeout_for_current_period)`. If it times out, produce and send. If signaled, re-evaluate period.
        *   `current_data = produce_data_for(_produced_data_type);`
        *   `Message response_msg = _communicator->new_message(Message::Type::RESPONSE, _produced_data_type, 0, serialized_data.data(), serialized_data.size());`
        *   `_communicator->send(response_msg, Protocol::Address(Ethernet::BROADCAST, 0)); // To Gateway Port 0`

### 3.3. `communicator.h`

*   Add `Component* _owner_component;` (passed in constructor).
*   Store `_component_port;` (the component's unique port, e.g., >=2, passed in constructor).
*   The `Communicator` constructor should register with the `Protocol` to observe its `_component_port` AND also `INTERNAL_BROADCAST_PORT` (Port 1).
*   **`Communicator::update(typename Channel::Observed* obs, typename Channel::Observer::Observing_Condition condition, Buffer* buf)`:**
    *   `// 'condition' here is the Port for which the message was received/notified.`
    *   `if (!buf) { Observer::update(condition, nullptr); return; } // Handle close signal, pass original condition`
    *   Deserialize message from `buf` into a local `Message msg_temp;`

    *   **If `_owner_component` is Gateway (and `condition == GATEWAY_PORT` (0)):**
        *   `Observer::update(condition, buf); // Pass buffer & original condition to unblock receive() for relay`
        *   `return;`

    *   **If `_owner_component` is Producer (and `condition == INTERNAL_BROADCAST_PORT` (1)):**
        *   If `msg_temp.message_type() == Message::Type::INTEREST` and `msg_temp.unit_type() == _owner_component->_produced_data_type`:
            *   `_owner_component->handle_received_interest(msg_temp.period());`
            *   `_channel->free(buf);`
        *   Else (not a relevant INTEREST for this producer on Port 1):
            *   `_channel->free(buf);`
        *   `return;`

    *   **If `_owner_component` is Consumer (and `condition == INTERNAL_BROADCAST_PORT` (1)):**
        *   If `msg_temp.message_type() == Message::Type::RESPONSE` and `msg_temp.unit_type() == _owner_component->_interested_data_type`:
            *   `current_time_us = /* get current time in microseconds */;`
            *   `if (current_time_us - _owner_component->_last_accepted_response_time_us >= _owner_component->_interested_period_us)`:
                *   `_owner_component->_last_accepted_response_time_us = current_time_us;`
                *   `Observer::update(condition, buf); // Pass buffer & original condition`
                *   `return;`
            *   Else (too early):
                *   `db<Communicator>(INF) << "Discarding early RESPONSE for type " << msg_temp.unit_type();`
                *   `_channel->free(buf);`
        *   Else (not a relevant RESPONSE for this consumer on Port 1):
            *   `_channel->free(buf);`
        *   `return;`

    *   `// Optional: Handle messages on _component_port if direct component-to-component messaging is ever added`
    *   `// if (condition == _component_port) { ... }`

    *   `_channel->free(buf); // Default: If not processed by any rule above or wrong port for this component type`

*   **`Communicator::receive(Message* message)`:**
    *   `Buffer* buf = Observer::updated(); // This will now also provide the condition if Observer pattern supports it` 
    *   `// If Observer::updated() doesn't return condition, then update() should store it if needed by receive.`
    *   `// For this plan, assume update() handles filtering and only passes buffers that should unblock receive.`
    *   If `buf` is not null, deserialize into `*message`.

### 3.4. `GatewayComponent.h` (Derived from `Component`)

*   **Constructor:**
    *   Initialize its `_communicator` to listen by observing `GATEWAY_PORT` (0) on the `Protocol`.
        `// _communicator = new Communicator(this, _vehicle->protocol(), 0); // Port 0 for Gateway`
        `// _communicator->observe(_vehicle->protocol(), GATEWAY_PORT);`
*   **Main Logic (e.g., in a `run()` method or similar):**
    ```cpp
    // void GatewayComponent::run() { // Or however its main loop is structured
    //     Message received_msg;
    //     while (_running.load()) { // _running is a Component base member
    //         if (_communicator->receive(&received_msg)) { // Receives any message from Port 0
    //             db<GatewayComponent>(INF) << "Gateway received msg on Port 0, type: " 
    //                                       << (int)received_msg.message_type() 
    //                                       << ", unit_type: " << (int)received_msg.unit_type()
    //                                       << " from " << received_msg.origin().to_string() << ". Relaying to Port 1.";
    //             _vehicle->protocol()->internal_broadcast_relay(received_msg); 
    //         }
    //     }
    // }
    ```
*   **Clarification for Relay:** The `GatewayComponent` receives a message. It then needs to trigger a send on the `Protocol` layer that targets `Protocol::Address(MAC_VEHICLE, INTERNAL_BROADCAST_PORT_1)`. The *payload* of this send should be the *original received message buffer*. The `Protocol` layer, upon receiving this for Port 1, would then notify all its observers registered for Port 1.

### 3.5. `protocol.h`

*   **Port Definitions:**
    *   `static const Protocol::Port GATEWAY_PORT = 0;`
    *   `static const Protocol::Port INTERNAL_BROADCAST_PORT = 1;`
*   **`Protocol::update()` (called by NIC/Engine when frame arrives):**
    *   Deserialize Ethernet frame to get destination MAC and `Protocol::Header` (which includes destination port `actual_dest_port`).
    *   If `dest_mac == _nic->address()` (or broadcast):
        *   If `actual_dest_port == GATEWAY_PORT` (0):
            *   `_observed.notify(GATEWAY_PORT, buf); // Notify observers of Port 0`
        *   Else if `actual_dest_port >= MIN_COMPONENT_PORT` (e.g. 2) (i.e. a specific component port):
            *   `_observed.notify(actual_dest_port, buf); // Notify observers of that specific port`
        *   Else (`actual_dest_port == INTERNAL_BROADCAST_PORT` (1) or other special cases from external network - typically unexpected for Port 1 directly from outside):
            *   `// Potentially log unexpected message for Port 1 from external network`
            *   `_channel->free(buf);`
*   **`Protocol::send(const Address & from, const Address & to, const char * payload, unsigned int size)`:**
    *   This is used by Communicators.
    *   If `to.port() == GATEWAY_PORT` and `to.paddr() == Ethernet::BROADCAST`, it's an external message.
    *   If `to.port() == INTERNAL_BROADCAST_PORT` and `to.paddr() == _nic->address()`: this is the Gateway *initiating* an internal relay. `Protocol` should take this buffer and effectively re-distribute it to all observers listening on `INTERNAL_BROADCAST_PORT`.
*   **Handling internal broadcasts:**
    *   Protocol identifies messages sent to `INTERNAL_BROADCAST_PORT` (Port 1)
    *   For these messages, Protocol uses `_observed.notifyInternalBroadcast()` to clone and distribute the message buffer to all observers of Port 1
    *   The `notifyInternalBroadcast()` method in the Observer pattern handles buffer cloning and distribution
    *   A buffer cloning function is provided to `notifyInternalBroadcast()` to create copies of the original buffer for each observer
    *   The original message's origin is preserved during this process

### 3.6. `nic.h`, `socketEngine.h`, `sharedMemoryEngine.h`

*   No direct changes anticipated for P3 logic, but ensure they work correctly with new porting scheme.
*   **Linter Errors:** Remind user: "Please remember to address the linter errors related to include paths (e.g., for POSIX headers like `unistd.h`, `pthread.h`, etc.) by configuring your development environment's include paths correctly. This is crucial for compilation and proper functioning on your target POSIX platform."

### 3.7. `observed.h`, `observer.h`

*   The existing `Conditional_Data_Observer<Buffer, Port>` mechanism (where `Port` is the `Observing_Condition`) is used by `Communicator` to observe `Protocol`.
    *   Gateway's `Communicator` observes `Protocol` by registering for the condition `GATEWAY_PORT` (0).
    *   Producers/Consumers' `Communicator`s will register with `Protocol` to observe (be notified for) messages on their unique `_component_port` (e.g., >=2) AND also for messages associated with the condition `INTERNAL_BROADCAST_PORT` (1).
    *   The `Protocol::internal_broadcast_relay` method will use `_observed.notify(INTERNAL_BROADCAST_PORT, cloned_buffer)` to trigger updates for all Observers (Communicators) that subscribed to `INTERNAL_BROADCAST_PORT`.

## 4. Implementation Order & Milestones

1.  **Setup & Basic Definitions:**
    *   Resolve linter/include path issues.
    *   Finalize `DataTypeId` enum and `Message::Type` enum (remove `REG_PRODUCER`).
    *   Define constants for `GATEWAY_PORT` and `INTERNAL_BROADCAST_PORT`.

2.  **Core `Communicator` and `Component` Modifications:**
    *   Update `Communicator::update` with new filtering logic for Consumer, Producer, and Gateway roles based on the `Observing_Condition` (port) from the `Protocol` notification and message content.
    *   In `Communicator` constructor: register with `Protocol` for both the component's unique port and `INTERNAL_BROADCAST_PORT`.
    *   Modify `Component` base for single interest (Consumer) and single production type (Producer), ensuring their `Communicator`s are set up correctly. Remove dispatcher thread logic.
    *   Implement direct data callback invocation in Consumer's main logic after `_communicator->receive()`.

3.  **Gateway Implementation:**
    *   Implement `GatewayComponent`.
    *   Its `Communicator` listens by observing `GATEWAY_PORT` (0) on the `Protocol`.
    *   Its main logic loop calls `_communicator->receive()`, and upon receiving a message, calls `_vehicle->protocol()->internal_broadcast_relay(received_msg)`.
    *   Update `Protocol` to implement `internal_broadcast_relay` which notifies all observers of `INTERNAL_BROADCAST_PORT` (1).

4.  **Consumer Component Implementation:**
    *   Derived consumer component sets its single interest (`DataTypeId`, `period`, callback).
    *   Sends `INTEREST` to `GATEWAY_PORT`.
    *   Its `Communicator` listens on `INTERNAL_BROADCAST_PORT` and applies filters.

5.  **Producer Component Implementation:**
    *   Derived producer component sets its produced `DataTypeId`.
    *   Its `Communicator` listens on `INTERNAL_BROADCAST_PORT` for matching `INTEREST`s.
    *   Implements GCD logic and periodic `RESPONSE` sending thread.
    *   Sends `RESPONSE` to `GATEWAY_PORT`.

6.  **Integration Test:**
    *   Consumer sends `INTEREST`.
    *   Gateway receives and relays `INTEREST`.
    *   Producer receives relayed `INTEREST`, calculates GCD, starts sending `RESPONSE`s.
    *   Gateway receives `RESPONSE`s and relays them.
    *   Consumer receives relayed `RESPONSE`s, filters by period, and callback is invoked.

7.  **Refinement & Robustness:**
    *   Thorough testing of thread safety (especially producer's GCD updates and response thread).
    *   Error handling and logging.

## 5. Open Questions/Considerations

*   **Protocol's `internal_broadcast_relay`:** This needs careful design in `Protocol.h` to ensure efficient and correct distribution of the relayed message buffer (with cloning) to all `Communicator`s listening on `INTERNAL_BROADCAST_PORT`.
*   **Message Serialization for Relay:** When the Gateway relays, it's essentially taking a received buffer and causing it to be re-processed by the `Protocol` layer for internal distribution. Ensure the buffer is handled correctly.
*   **Origin of Relayed Messages:** The `origin` field in relayed messages should remain that of the original sender, not the Gateway. The `internal_broadcast_relay` mechanism must preserve this.
*   **Stopping Threads:** Ensure robust signaling and joining of `_component_dispatcher_thread` and `_producer_response_thread`. Semaphores are good for the producer thread.

This revised plan significantly simplifies the P3 design. 