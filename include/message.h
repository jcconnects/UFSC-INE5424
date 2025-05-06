#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstdint>
#include <vector>
#include <chrono>

#include "traits.h"

// foward declarations
class SocketEngine;

class SharedmemoryEngine;

template <typename ExternalEngine, typename InternalEngine>
class NIC;

template <typename NIC>
class Protocol;

template <typename Channel>
class Communicator;

class Message {

    public:
        /* Definitions */
        enum class Type : std::uint8_t {
            INTEREST,
            RESPONSE,
            // PTP,
            // JOIN,
        };

        typedef Protocol<NIC<SocketEngine, SharedMemoryEngine>>::Address Origin;
        // static const unsigned int MAC_SIZE;

        /* Methods */
        ~Message() = default;

        // Getters
        const Type& message_type() const;
        const Origin& origin() const;
        const std::uint64_t& timestamp() const;
        const std::uint32_t& unit_type() const;
        const unsigned int period() const;
        const std::uint8_t* value() const;
        const void* data() const;
        const unsigned int size() const;
        static Message deserialize(const void* serialized, const unsigned int size);
    
    private:
        friend class Communicator<Protocol<NIC<SocketEngine, SharedMemoryEngine>>>;

        // Private constructors (idea is that only communicator and Message::deserealize are allowed to create messages)
        Message() = default;
        Message(Type message_type, const Origin& origin, std::uint32_t unit_type, unsigned int period = 0, const void* value_data = nullptr, const unsigned int value_size = 0);
    

        /* Setters
            (idea is that only Message::deserealize method can set message attributes,
             i.e., once a message is created, it will be constant through the whole execution) */
        void message_type(const Type message_type);
        void origin(const Origin& addr);
        void timestamp(const std::uint64_t& timestamp);
        void unit_type(const std::uint32_t& type);
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
        static Origin extract_address(const std::uint8_t* data, unsigned int& offset, unsigned int size);
        static std::uint8_t extract_uint8t(const std::uint8_t* data, unsigned int& offset, unsigned int size);
        static std::uint32_t extract_uint32t(const std::uint8_t* data, unsigned int& offset, unsigned int size);
        static std::uint64_t extract_uint64t(const std::uint8_t* data, unsigned int& offset, unsigned int size);

    private:
        /* Attributes */
        Type _message_type;
        Origin _origin;
        std::uint64_t _timestamp;
        std::uint32_t _unit_type;
        std::uint32_t _period;                 // INTEREST
        std::vector<std::uint8_t> _value;      // RESPONSE
        // std::vector<std::uint8_t> _mac;     // INTEREST and RESPONSE

        // Serialized message buffer
        std::vector<std::uint8_t> _serialized_data;

};

/********* Message Implementation *********/
// const unsigned int Message::MAC_SIZE = Traits<Message>::MAC_SIZE;

Message::Message(Type message_type, const Origin& origin, std::uint32_t unit_type, unsigned int period, const void* value_data, const unsigned int value_size) {
    auto now_system = std::chrono::system_clock::now();
    _timestamp = std::chrono::duration_cast<std::chrono::microseconds>(now_system.time_since_epoch()).count();

    _message_type = message_type;
    _origin = origin;
    _unit_type = unit_type;
    _period = period;
    
    if (_message_type == Type::RESPONSE && !value_data && value_size > 0)
        value(value_data, value_size);
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

const std::uint32_t& Message::unit_type() const {
    return _unit_type;
}

const unsigned int Message::period() const {
    return static_cast<unsigned int>(_period);
}

const std::uint8_t* Message::value() const {
    return _value.data();
}

const void* Message::data() const {
    // Ensures message is serialized
    const_cast<Message*>(this)->serialize();

    // Returns serialized message
    return static_cast<const void*>(_serialized_data.data());
}

const unsigned int Message::size() const {
    // Ensure message is serialized
    const_cast<Message*>(this)->serialize();

    // Returns serialized message size
    return static_cast<const unsigned int>(_serialized_data.size());
}


Message Message::deserialize(const void* serialized, const unsigned int size) {
    const std::uint8_t* bytes = static_cast<const std::uint8_t*>(serialized);

    Message msg;
    unsigned int offset = 0;

    msg.message_type(static_cast<Type>(extract_uint8t(bytes, offset, size)));
    msg.origin(extract_address(bytes, offset, size));
    msg.timestamp(extract_uint64t(bytes, offset, size));
    msg.unit_type(extract_uint32t(bytes, offset, size));

    if (msg.message_type() == Type::INTEREST) {
        msg.period(extract_uint32t(bytes, offset, size));
    } else if (msg.message_type() == Type::RESPONSE) {
        unsigned int value_len = size - offset;
        if (value_len > 0) {
            msg.value(bytes + offset, value_len);
            offset += value_len;
        } else {
            msg.value(nullptr, 0);
        }
    }

    msg.serialize();
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

void Message::unit_type(const std::uint32_t& unit_type) {
    _unit_type = unit_type;
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

    append_uint8t(static_cast<uint8_t>(_message_type)); // C++ can't handle bits
    append_address(_origin);
    append_uint64t(_timestamp);
    append_uint32t(_unit_type);

    if (_message_type == Type::INTEREST)
        append_uint32t(_period);
    else if (_message_type == Type::RESPONSE)
        append_vector(_value);
}

void Message::append_address(const Origin& addr) {
    for (int i = 0; i < 6; ++i)
        _serialized_data.push_back(_origin.paddr().bytes[i]);

    _serialized_data.push_back(static_cast<uint8_t>(_origin.port() >> 8));
    _serialized_data.push_back(static_cast<uint8_t>(_origin.port() & 0xFF));
}

void Message::append_uint8t(const std::uint8_t& value) {
    _serialized_data.push_back(value);
}

void Message::append_uint32t(const std::uint32_t& value) {
    for (int i = 3; i >= 0; --i){
        _serialized_data.push_back((value >> (8 * i)) & 0xFF);
    }
}

void Message::append_uint64t(const std::uint64_t& value) {
    for (int i = 7; i >= 0; --i){
        _serialized_data.push_back((value >> (8 * i)) & 0xFF);
    }
}

void Message::append_vector(const std::vector<std::uint8_t>& v) {
    _serialized_data.insert(_serialized_data.end(), v.begin(), v.end());
}

Message::Origin Message::extract_address(const std::uint8_t* data, unsigned int& offset, unsigned int size) {
    if (offset + 8 > size)
        throw std::runtime_error("Truncated origin address");

    Ethernet::Address mac = {};
    for (int i = 0; i < 6; ++i)
        mac.bytes[i] = data[offset++];

    std::uint16_t port = (static_cast<uint16_t>(data[offset]) << 8) | static_cast<uint16_t>(data[offset + 1]);
    offset += 2;

    return Origin(mac, port);
}

std::uint8_t Message::extract_uint8t(const std::uint8_t* data, unsigned int& offset, unsigned int size) {
    if (offset + 1 > size)
        throw std::runtime_error("Out of bounds");

    return data[offset++];
}

std::uint32_t Message::extract_uint32t(const std::uint8_t* data, unsigned int& offset, unsigned int size) {
    if (offset + 4 > size)
        throw std::runtime_error("Out of bounds");

    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i)
        value = (value << 8) | data[offset++];
    
    return value;
}

std::uint64_t Message::extract_uint64t(const std::uint8_t* data, unsigned int& offset, unsigned int size) {
    if (offset + 8 > size)
        throw std::runtime_error("Out of bounds");

    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i)
        value = (value << 8) | data[offset++];
    
    return value;
}

#endif // MESSAGE_H