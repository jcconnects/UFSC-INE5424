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

class Agent;

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
        typedef Protocol<NIC<SocketEngine, SharedMemoryEngine>>::Port AddrPort;
        typedef Protocol<NIC<SocketEngine, SharedMemoryEngine>>::Physical_Address MAC_Address;
        typedef std::vector<std::uint8_t> ByteArray;
        typedef std::chrono::microseconds Microseconds;
        typedef std::uint32_t Unit;
        // static const unsigned int MAC_SIZE;

        /* Methods */
        Message() = default;
        ~Message() = default;

        // Getters
        const Type& message_type() const;
        const Origin& origin() const;
        const Microseconds& timestamp() const;
        const Unit& unit_type() const;
        const Microseconds& period() const;
        const std::uint8_t* value() const;
        const unsigned int value_size() const;
        const void* data() const;
        const unsigned int size() const;
        static Message deserialize(const void* serialized, const unsigned int size);
    
    private:
        friend class Agent;

        // Private constructors (idea is that only communicator and Message::deserealize are allowed to create messages)
        Message(Type message_type, const Origin& origin, Unit unit_type, Microseconds period = Microseconds::zero(), const void* value_data = nullptr, const unsigned int value_size = 0);
    
        /* Setters
            (idea is that only Message::deserealize method can set message attributes,
             i.e., once a message is created, it will be constant through the whole execution) */
        void message_type(const Type message_type);
        void origin(const Origin addr);
        void timestamp(const Microseconds timestamp);
        void unit_type(const Unit type);
        void period(const Microseconds period);
        void value(const void* data, const unsigned int size);
        void serialize();

        // Serialization auxiliar methods
        void append_origin();
        void append_type();
        void append_unit();
        void append_microseconds(const Microseconds& value); 
        void append_value();

        // Deserialization auxiliar methods
        static Origin extract_origin(const std::uint8_t* data, unsigned int& offset, unsigned int size);
        static Type extract_type(const std::uint8_t* data, unsigned int& offset, unsigned int size);
        static Unit extract_unit(const std::uint8_t* data, unsigned int& offset, unsigned int size);
        static Microseconds extract_microseconds(const std::uint8_t* data, unsigned int& offset, unsigned int size);

    private:
        /* Attributes */
        Type _message_type;
        Origin _origin;
        Microseconds _timestamp;
        Unit _unit_type;
        Microseconds _period;     // INTEREST
        ByteArray _value;        // RESPONSE
        // ByteArray _mac;       // INTEREST and RESPONSE

        // Serialized message buffer
        ByteArray _serialized_data;

};

/********* Message Implementation *********/
// const unsigned int Message::MAC_SIZE = Traits<Message>::MAC_SIZE;

Message::Message(Type message_type, const Origin& origin, std::uint32_t unit_type, std::chrono::microseconds period, const void* value_data, const unsigned int value_size) {
    auto now_system = std::chrono::system_clock::now();
    _timestamp = Microseconds(std::chrono::duration_cast<std::chrono::microseconds>(now_system.time_since_epoch()).count());

    _message_type = message_type;
    _origin = origin;
    _unit_type = unit_type;
    _period = period;
    
    if (_message_type == Type::RESPONSE)
        value(value_data, value_size);
}

const Message::Type& Message::message_type() const {
    return _message_type;
}

const Message::Origin& Message::origin() const {
    return _origin;
}

const Message::Microseconds& Message::timestamp() const {
    return _timestamp;
}

const Message::Unit& Message::unit_type() const {
    return _unit_type;
}

const Message::Microseconds& Message::period() const {
    return _period;
}

const std::uint8_t* Message::value() const {
    return _value.data();
}

const unsigned int Message::value_size() const {
    return _value.size();
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

    msg.message_type(extract_type(bytes, offset, size));
    msg.origin(extract_origin(bytes, offset, size));
    msg.timestamp(extract_microseconds(bytes, offset, size));
    msg.unit_type(extract_unit(bytes, offset, size));

    if (msg.message_type() == Type::INTEREST) {
        msg.period(extract_microseconds(bytes, offset, size));
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

void Message::origin(const Origin addr) {
    _origin = addr;
}

void Message::timestamp(const Microseconds timestamp) {
    _timestamp = timestamp;
}

void Message::unit_type(const Unit unit_type) {
    _unit_type = unit_type;
}

void Message::period(const Microseconds period) {
    _period = period;
}

void Message::value(const void* data, const unsigned int size) {
    if (data && size > 0) {
        _value.resize(size);
        std::memcpy(_value.data(), data, size);
    }
}

void Message::serialize() {
    // Clear before any operations
    _serialized_data.clear();

    append_type(); // C++ can't handle bits
    append_origin();
    append_microseconds(_timestamp);
    append_unit();

    if (_message_type == Type::INTEREST)
        append_microseconds(_period);
    else if (_message_type == Type::RESPONSE)
        append_value();
}

void Message::append_origin() {
    for (int i = 0; i < 6; ++i)
        _serialized_data.push_back(_origin.paddr().bytes[i]);

    _serialized_data.push_back(static_cast<uint8_t>(_origin.port() >> 8));
    _serialized_data.push_back(static_cast<uint8_t>(_origin.port() & 0xFF));
}

void Message::append_type() {
    _serialized_data.push_back(static_cast<std::uint8_t>(_message_type));
}

void Message::append_unit() {
    for (int i = 3; i >= 0; --i){
        _serialized_data.push_back((_unit_type >> (8 * i)) & 0xFF);
    }
}

void Message::append_microseconds(const Microseconds& value) {
    std::int64_t bytes = value.count();

    for (int i = 7; i >= 0; --i){
        _serialized_data.push_back((bytes >> (8 * i)) & 0xFF);
    }
}

void Message::append_value() {
    _serialized_data.insert(_serialized_data.end(), _value.begin(), _value.end());
}

Message::Origin Message::extract_origin(const std::uint8_t* data, unsigned int& offset, unsigned int size) {
    if (offset + 8 > size)
        throw std::runtime_error("Truncated origin address");

    MAC_Address mac = {};
    for (int i = 0; i < 6; ++i)
        mac.bytes[i] = data[offset++];

    AddrPort port = (static_cast<AddrPort>(data[offset]) << 8) | static_cast<AddrPort>(data[offset + 1]);
    offset += 2;

    return Origin(mac, port);
}

Message::Type Message::extract_type(const std::uint8_t* data, unsigned int& offset, unsigned int size) {
    if (offset + 1 > size)
        throw std::runtime_error("Out of bounds");

    return static_cast<Type>(data[offset++]);
}

Message::Unit Message::extract_unit(const std::uint8_t* data, unsigned int& offset, unsigned int size) {
    if (offset + 4 > size)
        throw std::runtime_error("Out of bounds");

    Message::Unit value = 0;
    for (int i = 0; i < 4; ++i)
        value = (value << 8) | data[offset++];
    
    return value;
}

Message::Microseconds Message::extract_microseconds(const std::uint8_t* data, unsigned int& offset, unsigned int size) {
    if (offset + 8 > size)
        throw std::runtime_error("Out of bounds");

    std::int64_t value = 0;
    for (int i = 0; i < 8; ++i)
        value = (value << 8) | data[offset++];
    
    return Microseconds(value);
}

#endif // MESSAGE_H