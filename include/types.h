#ifndef TYPES_H
#define TYPES_H

// Forward declarations of template classes
template <typename Engine1, typename Engine2>
class NIC;

template <typename NIC> 
class Protocol;

template <typename Protocol>
class Communicator;

template <unsigned int MaxSize>
class Message;

// Forward declare Address before defining aliases
class Address;  // Protocol<NIC>::Address will be defined in protocol.h

// Concrete type definitions - without using inner types of incomplete classes
using SocketEngine = class SocketEngine;
using SharedMemoryEngine = class SharedMemoryEngine;
using TheNIC = NIC<SocketEngine, SharedMemoryEngine>;
using TheProtocol = Protocol<TheNIC>;
// TheAddress will be defined after Protocol is fully defined

// Constants
namespace Constants {
    constexpr unsigned int MAX_MESSAGE_SIZE = 1024; // Default value, adjust as needed
}

// The Message type will be defined in message.h using the constant
// using TheMessage = Message<Constants::MAX_MESSAGE_SIZE>;

#endif // TYPES_H 