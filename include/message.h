#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstdint>
#include <vector>
#include <chrono>
#include <cstring> // For std::memcpy, std::strlen
#include <stdexcept> // For std::runtime_error

#include "traits.h"
#include "teds.h" // Added for DataTypeId

// foward declarations
class SocketEngine;

class SharedMemoryEngine;

template <typename ExternalEngine, typename InternalEngine>
class NIC;

#include "protocol.h" // Add full include

template <typename Channel>
class Communicator;

class Message {

    public:
        /* Definitions */
        enum class Type : std::uint8_t {
            UNKNOWN = 0,
            INTEREST = 1,
            RESPONSE = 2
        };

        typedef Protocol<NIC<SocketEngine, SharedMemoryEngine>>::Address Origin;
        // static const unsigned int MAC_SIZE;

        /* Methods */
        ~Message() = default;

        // Getters
        const Type& message_type() const;
        const Origin& origin() const;
        const std::uint64_t& timestamp() const;
        const DataTypeId& unit_type() const; // Changed return type
        const unsigned int period() const;
        const std::uint8_t* value() const; // Returns pointer to internal vector data
        const unsigned int value_size() const; // Returns size of the value vector
        const void* data() const; // Serialized data
        const unsigned int size() const; // Serialized data size
        static Message deserialize(const void* serialized, const unsigned int size);

        // Public constructor for P3 message types, useful for Communicator and tests
        Message(Type message_type, const Origin& origin, DataTypeId unit_type, unsigned int period = 0, const void* value_data = nullptr, const unsigned int value_size = 0);
    
    private:
        friend class Communicator<Protocol<NIC<SocketEngine, SharedMemoryEngine>>>;

        // Private default constructor, used by deserialize
        Message() = default;
    

        /* Setters
            (idea is that only Message::deserealize method can set message attributes,
             i.e., once a message is created, it will be constant through the whole execution) */
        void message_type(const Type message_type);
        void origin(const Origin& addr);
        void timestamp(const std::uint64_t& timestamp);
        void unit_type(DataTypeId type); // Changed parameter type
        void period(const std::uint32_t& period);
        void value(const void* data, const unsigned int size);
        void serialize();

        // Serialization auxiliar methods
        void append_address(const Origin& origin);
        void append_uint8t(const std::uint8_t& value);
        void append_uint32t(const std::uint32_t& value);
        void append_uint64t(const std::uint64_t& value); 
        void append_vector(const std::vector<std::uint8_t>& v);

        // Deserialization auxiliar methods
        static Origin extract_address(const std::uint8_t* data, unsigned int& offset, unsigned int max_size); // Added max_size
        static std::uint8_t extract_uint8t(const std::uint8_t* data, unsigned int& offset, unsigned int max_size); // Added max_size
        static std::uint32_t extract_uint32t(const std::uint8_t* data, unsigned int& offset, unsigned int max_size); // Added max_size
        static std::uint64_t extract_uint64t(const std::uint8_t* data, unsigned int& offset, unsigned int max_size); // Added max_size

    private:
        /* Attributes */
        Type _message_type;
        Origin _origin;
        std::uint64_t _timestamp;
        DataTypeId _unit_type; // Changed type from std::uint32_t
        std::uint32_t _period;                 // INTEREST
        std::vector<std::uint8_t> _value;      // RESPONSE
        // std::vector<std::uint8_t> _mac;     // INTEREST and RESPONSE

        // Serialized message buffer
        std::vector<std::uint8_t> _serialized_data;

};

/********* Message Implementation *********/
// const unsigned int Message::MAC_SIZE = Traits<Message>::MAC_SIZE;

Message::Message(Type message_type, const Origin& origin, DataTypeId unit_type, unsigned int period, const void* value_data, const unsigned int value_size) 
: _message_type(message_type), _origin(origin), _unit_type(unit_type), _period(static_cast<std::uint32_t>(period)) {
    auto now_system = std::chrono::system_clock::now();
    _timestamp = std::chrono::duration_cast<std::chrono::microseconds>(now_system.time_since_epoch()).count();
    
    if (_message_type == Type::RESPONSE && value_data && value_size > 0) {
        this->value(value_data, value_size);
    }
    // Ensure _serialized_data is built upon construction if needed, or lazily.
    // For now, serialize() is called by data() and size() getters.
}

const Message::Type& Message::message_type() const {
    return _message_type;
}

const Message::Origin& Message::origin() const {
    return _origin;
}

const std::uint64_t& Message::timestamp() const {
    return _timestamp;
}

// Getter for DataTypeId
const DataTypeId& Message::unit_type() const {
    return _unit_type;
}

const unsigned int Message::period() const {
    return static_cast<unsigned int>(_period);
}

const std::uint8_t* Message::value() const {
    if (_value.empty()) {
        return nullptr;
    }
    return _value.data();
}

const unsigned int Message::value_size() const {
    return static_cast<unsigned int>(_value.size());
}

const void* Message::data() const {
    // Ensures message is serialized
    const_cast<Message*>(this)->serialize();
    if (_serialized_data.empty()) { // Should not happen if serialize works correctly
        return nullptr;
    }
    return static_cast<const void*>(_serialized_data.data());
}

const unsigned int Message::size() const {
    // Ensure message is serialized
    const_cast<Message*>(this)->serialize();
    return static_cast<unsigned int>(_serialized_data.size());
}


Message Message::deserialize(const void* serialized, const unsigned int size) {
    if (!serialized || size == 0) {
        throw std::runtime_error("Cannot deserialize from null or zero-size buffer");
    }
    const std::uint8_t* bytes = static_cast<const std::uint8_t*>(serialized);

    Message msg; // Uses default constructor
    unsigned int offset = 0;

    msg.message_type(static_cast<Type>(extract_uint8t(bytes, offset, size)));
    msg.origin(extract_address(bytes, offset, size));
    msg.timestamp(extract_uint64t(bytes, offset, size));
    // Deserialize as uint32_t then cast to DataTypeId
    msg.unit_type(static_cast<DataTypeId>(extract_uint32t(bytes, offset, size)));

    if (msg.message_type() == Type::INTEREST) {
        msg.period(extract_uint32t(bytes, offset, size));
    } else if (msg.message_type() == Type::RESPONSE) {
        if (offset < size) { // Check if there's remaining data for value
            unsigned int value_len = size - offset;
            msg.value(bytes + offset, value_len);
            // offset += value_len; // Not needed as it's the last field
        } else {
            msg.value(nullptr, 0); // No value data
        }
    }
    // Deserialized message's _serialized_data is not yet populated.
    // It will be populated on first call to data() or size().
    return msg;
}

void Message::message_type(const Type message_type) {
    _message_type = message_type;
}

void Message::origin(const Origin& addr) {
    _origin = addr;
}

void Message::timestamp(const std::uint64_t& timestamp) {
    _timestamp = timestamp;
}

// Setter for DataTypeId
void Message::unit_type(DataTypeId type) {
    _unit_type = type;
}

void Message::period(const std::uint32_t& period) {
    _period = period;
}

void Message::value(const void* data, const unsigned int size) {
    if (data && size > 0) {
        _value.resize(size);
        std::memcpy(_value.data(), data, size);
    } else {
        _value.clear();
    }
}

void Message::serialize() {
    // Clear before any operations
    _serialized_data.clear();

    append_uint8t(static_cast<uint8_t>(_message_type));
    append_address(_origin);
    append_uint64t(_timestamp);
    // Serialize DataTypeId as underlying uint32_t
    append_uint32t(static_cast<std::uint32_t>(_unit_type));

    if (_message_type == Type::INTEREST) {
        append_uint32t(_period);
    } else if (_message_type == Type::RESPONSE) {
        if (!_value.empty()) {
            append_vector(_value);
        }
    }
}

void Message::append_address(const Origin& addr) {
    // Physical address (MAC)
    for (int i = 0; i < 6; ++i) { // Assuming MAC is 6 bytes
        _serialized_data.push_back(addr.paddr().bytes[i]);
    }
    // Port (assuming 2 bytes for port in Protocol::Address)
    _serialized_data.push_back(static_cast<uint8_t>(addr.port() >> 8));
    _serialized_data.push_back(static_cast<uint8_t>(addr.port() & 0xFF));
}

void Message::append_uint8t(const std::uint8_t& value) {
    _serialized_data.push_back(value);
}

void Message::append_uint32t(const std::uint32_t& value) {
    _serialized_data.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    _serialized_data.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    _serialized_data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    _serialized_data.push_back(static_cast<uint8_t>(value & 0xFF));
}

void Message::append_uint64t(const std::uint64_t& value) {
    _serialized_data.push_back(static_cast<uint8_t>((value >> 56) & 0xFF));
    _serialized_data.push_back(static_cast<uint8_t>((value >> 48) & 0xFF));
    _serialized_data.push_back(static_cast<uint8_t>((value >> 40) & 0xFF));
    _serialized_data.push_back(static_cast<uint8_t>((value >> 32) & 0xFF));
    _serialized_data.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    _serialized_data.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    _serialized_data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    _serialized_data.push_back(static_cast<uint8_t>(value & 0xFF));
}

void Message::append_vector(const std::vector<std::uint8_t>& v) {
    _serialized_data.insert(_serialized_data.end(), v.begin(), v.end());
}

// Updated extract methods with max_size check
Message::Origin Message::extract_address(const std::uint8_t* data, unsigned int& offset, unsigned int max_size) {
    if (offset + 8 > max_size) { // 6 bytes MAC + 2 bytes port
        throw std::runtime_error("Buffer too small to extract Origin address");
    }
    Ethernet::Address mac = {}; // Assuming Ethernet::Address is a struct/array
    for (int i = 0; i < 6; ++i) {
        mac.bytes[i] = data[offset++];
    }
    std::uint16_t port = (static_cast<std::uint16_t>(data[offset]) << 8) | static_cast<std::uint16_t>(data[offset + 1]);
    offset += 2;
    return Origin(mac, port);
}

std::uint8_t Message::extract_uint8t(const std::uint8_t* data, unsigned int& offset, unsigned int max_size) {
    if (offset + 1 > max_size) {
        throw std::runtime_error("Buffer too small to extract uint8_t");
    }
    return data[offset++];
}

std::uint32_t Message::extract_uint32t(const std::uint8_t* data, unsigned int& offset, unsigned int max_size) {
    if (offset + 4 > max_size) {
        throw std::runtime_error("Buffer too small to extract uint32_t");
    }
    std::uint32_t value = 0;
    value |= static_cast<std::uint32_t>(data[offset++]) << 24;
    value |= static_cast<std::uint32_t>(data[offset++]) << 16;
    value |= static_cast<std::uint32_t>(data[offset++]) << 8;
    value |= static_cast<std::uint32_t>(data[offset++]);
    return value;
}

std::uint64_t Message::extract_uint64t(const std::uint8_t* data, unsigned int& offset, unsigned int max_size) {
    if (offset + 8 > max_size) {
        throw std::runtime_error("Buffer too small to extract uint64_t");
    }
    std::uint64_t value = 0;
    value |= static_cast<std::uint64_t>(data[offset++]) << 56;
    value |= static_cast<std::uint64_t>(data[offset++]) << 48;
    value |= static_cast<std::uint64_t>(data[offset++]) << 40;
    value |= static_cast<std::uint64_t>(data[offset++]) << 32;
    value |= static_cast<std::uint64_t>(data[offset++]) << 24;
    value |= static_cast<std::uint64_t>(data[offset++]) << 16;
    value |= static_cast<std::uint64_t>(data[offset++]) << 8;
    value |= static_cast<std::uint64_t>(data[offset++]);
    return value;
}

#endif // MESSAGE_H