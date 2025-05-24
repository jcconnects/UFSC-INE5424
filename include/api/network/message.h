#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstdint>
#include <cstring>
#include <vector>
#include <chrono>
#include "api/util/debug.h"


template <typename Protocol>
class Message {

    public:
        /* Definitions */
        enum class Type : std::uint8_t {
            UNKNOWN = 0,
            INVALID = 1,
            INTEREST,
            RESPONSE,
            PTP,
            JOIN,
        };

        typedef typename Protocol::Address Origin;
        typedef typename Protocol::Physical_Address Physical_Address;
        typedef typename Protocol::Port Port;
        typedef std::uint32_t Unit;
        typedef std::vector<std::uint8_t> Array;
        typedef std::chrono::microseconds Microseconds;
        static constexpr Microseconds ZERO = Microseconds::zero();
        // static const unsigned int MAC_SIZE;

        /* Methods */
        Message(Type message_type, const Origin& origin, Unit unit, Microseconds period = ZERO, const void* value_data = nullptr, const unsigned int value_size = 0);
        Message(const Message& other);
        Message() = default;
        ~Message() = default;

        // Getters
        const Type& message_type() const;
        const Origin& origin() const;
        const Microseconds& timestamp() const;
        const Unit& unit() const;
        const Microseconds& period() const;
        const std::uint8_t* value() const;
        const unsigned int value_size() const;
        const void* data() const;
        const unsigned int size() const;
        static Message deserialize(const void* serialized, const unsigned int size);
        
    private:
        // Setters
        void message_type(const Type message_type);
        void origin(const Origin addr);
        void timestamp(const Microseconds timestamp);
        void unit(const Unit type);
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
        Unit _unit;
        Microseconds _period;     // INTEREST
        Array _value;             // RESPONSE
        // Array _mac;            // INTEREST and RESPONSE

        // Serialized message buffer
        Array _serialized_data;

};

/********* Message Implementation *********/
// const unsigned int Message::MAC_SIZE = Traits<Message>::MAC_SIZE;

template <typename Protocol>
Message<Protocol>::Message(Type message_type, const Origin& origin, Unit unit, Microseconds period, const void* value_data, const unsigned int value_size) :
    _message_type(Type::UNKNOWN),
    _origin(),
    _timestamp(ZERO),
    _period(ZERO),
    _value()
{
    auto now_system = std::chrono::system_clock::now();
    _timestamp = Microseconds(std::chrono::duration_cast<std::chrono::microseconds>(now_system.time_since_epoch()).count());

    _message_type = message_type;
    
    if (_message_type != Type::UNKNOWN && _message_type != Type::INVALID) {
        this->origin(origin);
        this->unit(unit);
        switch (_message_type) {
            case Type::INTEREST:
                this->period(period);
                break;
            case Type::RESPONSE:
                this->value(value_data, value_size);
                break;
            default:
                break;
        }
    }
}

template <typename Protocol>
Message<Protocol>::Message(const Message& other) {
    _message_type = other.message_type();
    _origin = other.origin();
    _timestamp = other.timestamp();
    _period = other.period();
    _unit = other.unit();
    value(other.value(), other.value_size());
    // TODO:: serialized data
}

template <typename Protocol>
const typename Message<Protocol>::Type& Message<Protocol>::message_type() const {
    return _message_type;
}

template <typename Protocol>
const typename Message<Protocol>::Origin& Message<Protocol>::origin() const {
    return _origin;
}

template <typename Protocol>
const typename Message<Protocol>::Microseconds& Message<Protocol>::timestamp() const {
    return _timestamp;
}

template <typename Protocol>
const typename Message<Protocol>::Unit& Message<Protocol>::unit() const {
    return _unit;
}

template <typename Protocol>
const typename Message<Protocol>::Microseconds& Message<Protocol>::period() const {
    return _period;
}

template <typename Protocol>
const std::uint8_t* Message<Protocol>::value() const {
    return _value.data();
}

template <typename Protocol>
const unsigned int Message<Protocol>::value_size() const {
    return _value.size();
}

template <typename Protocol>
const void* Message<Protocol>::data() const {
    // Ensures message is serialized
    const_cast<Message*>(this)->serialize();

    // Returns serialized message
    return static_cast<const void*>(_serialized_data.data());
}

template <typename Protocol>
const unsigned int Message<Protocol>::size() const {
    // Ensure message is serialized
    const_cast<Message*>(this)->serialize();

    // Returns serialized message size
    return static_cast<const unsigned int>(_serialized_data.size());
}

template <typename Protocol>
Message<Protocol> Message<Protocol>::deserialize(const void* serialized, const unsigned int size) {
    const std::uint8_t* bytes = static_cast<const std::uint8_t*>(serialized);

    Message msg;
    unsigned int offset = 0;

    msg.message_type(extract_type(bytes, offset, size));
    if (msg.message_type() != Type::UNKNOWN && msg.message_type() != Type::INVALID) {
        msg.origin(extract_origin(bytes, offset, size));
        msg.timestamp(extract_microseconds(bytes, offset, size));
        msg.unit(extract_unit(bytes, offset, size));

        switch (msg.message_type()) {
            case Type::INTEREST: {
                msg.period(extract_microseconds(bytes, offset, size));
                break;
            }
            case Type::RESPONSE: {
                unsigned int value_len = size - offset;
                if (value_len > 0) {
                    msg.value(bytes + offset, value_len);
                    offset += value_len;
                } else {
                    msg.message_type(Type::INVALID);
                }
                break;
            }
            default:
                break;
        }

        msg.serialize();
    }

    return msg;
}

template <typename Protocol>
void Message<Protocol>::message_type(const Type message_type) {
    _message_type = message_type;
}

template <typename Protocol>
void Message<Protocol>::origin(const Origin addr) {
    if (addr == Origin()){
        message_type(Type::INVALID);
        return;
    }

    _origin = addr;
}

template <typename Protocol>
void Message<Protocol>::timestamp(const Microseconds timestamp) {
    if (timestamp <= ZERO) {
        message_type(Type::INVALID);
        return;
    }

    _timestamp = timestamp;
}

template <typename Protocol>
void Message<Protocol>::unit(const Unit unit) {
    _unit = unit;
}

template <typename Protocol>
void Message<Protocol>::period(const Microseconds period) {
    if (period <= ZERO) {
        message_type(Type::INVALID);
        return;
    }

    _period = period;
}

template <typename Protocol>
void Message<Protocol>::value(const void* data, const unsigned int size) {
    if (!data || !size) {
        message_type(Type::INVALID);
        return;
    }

    _value.resize(size);
    std::memcpy(_value.data(), data, size);
}

template <typename Protocol>
void Message<Protocol>::serialize() {
    // Clear before any operations
    _serialized_data.clear();

    append_type();
    append_origin();
    append_microseconds(_timestamp);
    append_unit();

    if (_message_type == Type::INTEREST)
        append_microseconds(_period);
    else if (_message_type == Type::RESPONSE)
        append_value();
}

template <typename Protocol>
void Message<Protocol>::append_origin() {
    // TODO
}

template <typename Protocol>
void Message<Protocol>::append_type() {
    _serialized_data.push_back(static_cast<std::uint8_t>(_message_type));
}

template <typename Protocol>
void Message<Protocol>::append_unit() {
    // TODO
}

template <typename Protocol>
void Message<Protocol>::append_microseconds(const Microseconds& value) {
    // TODO
}

template <typename Protocol>
void Message<Protocol>::append_value() {
    _serialized_data.insert(_serialized_data.end(), _value.begin(), _value.end());
}

template <typename Protocol>
typename Message<Protocol>::Origin Message<Protocol>::extract_origin(const std::uint8_t* data, unsigned int& offset, unsigned int size) {
    unsigned int paddr_size = sizeof(Physical_Address);
    if (offset + paddr_size > size)
        return Origin();

    Physical_Address addr;
    std::memcpy(&addr, data + offset, paddr_size);
    offset += paddr_size;

    unsigned int port_size = sizeof(Port);
    if (offset + port_size > size)
        return Origin();

    Port port;
    std::memcpy(&port, data + offset, port_size);
    offset += port_size;

    return Origin(addr, port);
}

template <typename Protocol>
typename Message<Protocol>::Type Message<Protocol>::extract_type(const std::uint8_t* data, unsigned int& offset, unsigned int size) {
    if (offset + sizeof(std::uint8_t) > size)
        return Type::UNKNOWN;

    std::uint8_t raw_type = data[offset++];
    return static_cast<Type>(raw_type);
}

template <typename Protocol>
typename Message<Protocol>::Unit Message<Protocol>::extract_unit(const std::uint8_t* data, unsigned int& offset, unsigned int size) {
    unsigned int unit_size = sizeof(Unit);

    if (offset + unit_size > size)
        return Unit();

    Unit unit;
    std::memcpy(&unit, data + offset, unit_size);
    offset += unit_size;

    return unit;
}

template <typename Protocol>
typename Message<Protocol>::Microseconds Message<Protocol>::extract_microseconds(const std::uint8_t* data, unsigned int& offset, unsigned int size) {
    unsigned int rep_size = sizeof(Microseconds::rep);

    if (offset + rep_size > size)
        return ZERO;

    Microseconds::rep raw_value;
    std::memcpy(&raw_value, data + offset, rep_size);
    offset += rep_size;

    return Microseconds(raw_value);
}

#endif // MESSAGE_H