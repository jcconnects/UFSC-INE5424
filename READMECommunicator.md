# Communicator Class Implementation

## Overview

The Communicator class is a key component of the distributed communication system designed for autonomous systems communication, following the Observer-Observed design pattern as specified in the project requirements.

## Design Principles

### Key Design Characteristics
- Uses a template-based design for flexibility
- Implements asynchronous message communication
- Supports broadcast messaging
- Integrates with the Concurrent Observer pattern
- Provides a unified API for message sending and receiving

## Class Template Structure

```cpp
template <typename Channel>
class Communicator: public Concurrent_Observer<typename Channel::Buffer, typename Channel::Port>
```

### Template Type Parameters
- `Channel`: Represents the communication protocol/network interface
  - Must provide the following type definitions:
    - `Buffer`: Type for message buffers
    - `Address`: Type representing network addresses
    - `Port`: Type representing communication ports

## Key Methods

### Constructor
```cpp
Communicator(Channel* channel, Address address)
```
- Attaches the communicator to a specific channel and address
- Registers as an observer with the communication channel

### Destructor
```cpp
~Communicator()
```
- Detaches the communicator from the channel

### Send Method
```cpp
bool send(const Message* message)
```
- Sends a message via the communication channel
- Uses broadcast addressing
- Returns true if message transmission was successful

### Receive Method
```cpp
bool receive(Message* message)
```
- Blocks until a message is received
- Extracts message data from the communication buffer
- Manages buffer reference counting
- Returns true if a message was successfully received

## Error Handling and Safety

- Null pointer checks for messages
- Reference counting for buffer management
- Thread-safe communication via the Observer pattern
- Graceful handling of communication errors

## Implementation Details

### Buffer Management
- Uses atomic reference counting for thread-safe buffer lifecycle
- Automatically deletes buffers when reference count reaches zero

### Asynchronous Communication
- Leverages `Concurrent_Observer::updated()` for blocking receive
- Supports non-blocking communication patterns

## Testing Strategy

The accompanying test script (`test_communicator.cpp`) covers:
1. Communicator creation and destruction
2. Message sending functionality
3. Message receiving mechanism
4. Concurrent communication scenarios

### Test Scenarios
- Single message send/receive
- Multiple communicators
- Concurrent sending and receiving
- Edge cases like null messages

## Integration Notes

- Compatible with the provided stub implementations
- Can be integrated with various communication protocols
- Follows the project's observer-observed communication model

## Performance Considerations

- Minimal overhead for message passing
- Uses lightweight synchronization mechanisms
- Designed for real-time systems with low-latency requirements

## Limitations and Future Improvements

- Current implementation assumes messages smaller than MTU
- Could be extended to support more complex routing mechanisms
- Potential for adding more robust error handling

## Example Usage

```cpp
ProtocolStub protocol;
ProtocolStub::Address address("vehicle_a", 1234);
Communicator<ProtocolStub> communicator(&protocol, address);

// Sending a message
Message msg("Hello, autonomous system!");
communicator.send(&msg);

// Receiving a message
Message received_msg;
communicator.receive(&received_msg);
```

## Dependencies

- Requires implementation of:
  - `Concurrent_Observer` template
  - `Message` class
  - Communication protocol stub/implementation

## Compliance

Fully compliant with the project specifications outlined in the professor's code guide.