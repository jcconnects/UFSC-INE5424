#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstdint>
#include <cstring>
#include <vector>
#include <chrono>
#include "../util/debug.h"
#include "api/framework/clock.h"  // Include Clock for synchronized timestamps

/**
 * @brief Template class for network messages with Clock integration
 * 
 * This Message class is integrated with the Clock singleton to provide
 * synchronized timestamps across the distributed system. When messages
 * are created, they automatically use the Clock's getSynchronizedTime()
 * method instead of local system time, ensuring temporal consistency
 * in a PTP-synchronized network.
 * 
 * Clock Integration Features:
 * - Automatic synchronized timestamping in constructor
 * - Static utility methods for getting synchronized timestamps
 * - Clock synchronization status checking
 * 
 * @tparam Protocol The network protocol type (e.g., Ethernet)
 */


template <typename Channel>
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
            STATUS,
        };

        typedef typename Channel::Address Origin;
        typedef typename Channel::Physical_Address Physical_Address;
        typedef typename Channel::Port Port;
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

        // Clock integration utilities
        static Microseconds getSynchronizedTimestamp();
        static bool isClockSynchronized();
        
        // Public Setters
        void message_type(const Type message_type);
        void origin(const Origin addr);
        void timestamp(const Microseconds timestamp);
        void unit(const Unit type);
        void period(const Microseconds period);
        void value(const void* data, const unsigned int size);

    private:
        // Internal helper
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

template <typename Channel>
Message<Channel>::Message(Type message_type, const Origin& origin, Unit unit, Microseconds period, const void* value_data, const unsigned int value_size) :
    _message_type(Type::UNKNOWN),
    _origin(),
    _timestamp(ZERO),
    _period(ZERO),
    _value()
{
    // Use Clock singleton for synchronized timestamps instead of system_clock
    auto& clock = Clock::getInstance();
    bool is_synchronized;
    auto synchronized_time = clock.getSynchronizedTime(&is_synchronized);
    _timestamp = Microseconds(std::chrono::duration_cast<std::chrono::microseconds>(synchronized_time.time_since_epoch()).count());
    _message_type = message_type;

    db<Message<Channel>>(TRC) << "Message::Message() called with type: " << static_cast<int>(_message_type) 
        << ", origin: " << origin.to_string() 
        << ", unit: " << unit 
        << ", period: " << period.count() 
        << ", value_size: " << value_size << "\n";
    
    if (_message_type != Type::UNKNOWN && _message_type != Type::INVALID) {
        this->origin(origin);
        this->unit(unit);
        switch (_message_type) {
            case Type::INTEREST:
                this->period(period);
                break;
            case Type::RESPONSE:
            case Type::STATUS:  // STATUS messages also use value data
                this->value(value_data, value_size);
                break;
            default:
                break;
        }
    }
}

template <typename Channel>
Message<Channel>::Message(const Message& other) {
    // Copy all basic fields first
    _message_type = other.message_type();
    _origin = other.origin();
    _timestamp = other.timestamp();
    _period = other.period();
    _unit = other.unit();
    
    // Copy value data if present
    if (other.value_size() > 0) {
        value(other.value(), other.value_size());
    }
    
    // Clear and regenerate serialized data to ensure consistency
    _serialized_data.clear();
    
    db<Message<Channel>>(TRC) << "Message::Message(const Message&) called with type: " 
        << static_cast<int>(_message_type) 
        << ", origin: " << _origin.to_string() 
        << ", unit: " << _unit 
        << ", period: " << _period.count() 
        << ", value_size: " << _value.size() << "\n";
        
    // Validate the copied message to detect corruption
    if (_message_type != Type::UNKNOWN && _message_type != Type::INVALID &&
        _message_type != Type::INTEREST && _message_type != Type::RESPONSE &&
        _message_type != Type::STATUS && _message_type != Type::PTP &&
        _message_type != Type::JOIN) {
        db<Message<Channel>>(ERR) << "Message copy constructor detected corrupted message type: " 
                                  << static_cast<int>(_message_type) << " - marking as INVALID\n";
        _message_type = Type::INVALID;
    }
}

template <typename Channel>
const typename Message<Channel>::Type& Message<Channel>::message_type() const {
    return _message_type;
}

template <typename Channel>
const typename Message<Channel>::Origin& Message<Channel>::origin() const {
    return _origin;
}

template <typename Channel>
const typename Message<Channel>::Microseconds& Message<Channel>::timestamp() const {
    return _timestamp;
}

template <typename Channel>
const typename Message<Channel>::Unit& Message<Channel>::unit() const {
    return _unit;
}

template <typename Channel>
const typename Message<Channel>::Microseconds& Message<Channel>::period() const {
    return _period;
}

template <typename Channel>
const std::uint8_t* Message<Channel>::value() const {
    return _value.data();
}

template <typename Channel>
const unsigned int Message<Channel>::value_size() const {
    return _value.size();
}

template <typename Channel>
const void* Message<Channel>::data() const {
    // Ensures message is serialized
    const_cast<Message*>(this)->serialize();

    // Returns serialized message
    return static_cast<const void*>(_serialized_data.data());
}

template <typename Channel>
const unsigned int Message<Channel>::size() const {
    // Ensure message is serialized
    const_cast<Message*>(this)->serialize();

    // Returns serialized message size
    return static_cast<const unsigned int>(_serialized_data.size());
}

template <typename Channel>
Message<Channel> Message<Channel>::deserialize(const void* serialized, const unsigned int size) {
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
            case Type::RESPONSE: 
            case Type::STATUS: {  // STATUS messages also have value data
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
        
        db<Message<Channel>>(TRC) << "Message::deserialize() - type: " << static_cast<int>(msg.message_type()) 
                                  << ", origin: " << msg.origin().to_string() 
                                  << ", unit: " << msg.unit() 
                                  << ", input size: " << size 
                                  << ", final offset: " << offset << "\n";
    } else {
        db<Message<Channel>>(WRN) << "Message::deserialize() - failed to deserialize message of size " << size << "\n";
    }

    return msg;
}

template <typename Channel>
void Message<Channel>::message_type(const Type message_type) {
    _message_type = message_type;
}

template <typename Channel>
void Message<Channel>::origin(const Origin addr) {
    _origin = addr;
}

template <typename Channel>
void Message<Channel>::timestamp(const Microseconds timestamp) {
    if (timestamp <= ZERO) {
        message_type(Type::INVALID);
        return;
    }

    _timestamp = timestamp;
}

template <typename Channel>
void Message<Channel>::unit(const Unit unit) {
    _unit = unit;
}

template <typename Channel>
void Message<Channel>::period(const Microseconds period) {
    if (period <= ZERO) {
        message_type(Type::INVALID);
        return;
    }

    _period = period;
}

template <typename Channel>
void Message<Channel>::value(const void* data, const unsigned int size) {
    if (!data || !size) {
        return;
    }

    _value.resize(size);
    std::memcpy(_value.data(), data, size);
}

template <typename Channel>
void Message<Channel>::serialize() {
    // Clear before any operations
    _serialized_data.clear();

    append_type();
    append_origin();
    append_microseconds(_timestamp);
    append_unit();

    if (_message_type == Type::INTEREST)
        append_microseconds(_period);
    else if (_message_type == Type::RESPONSE || _message_type == Type::STATUS)
        append_value();
        
    db<Message<Channel>>(TRC) << "Message::serialize() - type: " << static_cast<int>(_message_type) 
                              << ", origin: " << _origin.to_string() 
                              << ", unit: " << _unit 
                              << ", serialized size: " << _serialized_data.size() << "\n";
}

template <typename Channel>
void Message<Channel>::append_origin() {
    // Serialize physical address
    const auto& paddr = _origin.paddr();
    const std::uint8_t* paddr_bytes = reinterpret_cast<const std::uint8_t*>(&paddr);
    _serialized_data.insert(_serialized_data.end(), paddr_bytes, paddr_bytes + sizeof(Physical_Address));
    
    // Serialize port
    const auto& port = _origin.port();
    const std::uint8_t* port_bytes = reinterpret_cast<const std::uint8_t*>(&port);
    _serialized_data.insert(_serialized_data.end(), port_bytes, port_bytes + sizeof(Port));
}

template <typename Channel>
void Message<Channel>::append_type() {
    _serialized_data.push_back(static_cast<std::uint8_t>(_message_type));
}

template <typename Channel>
void Message<Channel>::append_unit() {
    const std::uint8_t* unit_bytes = reinterpret_cast<const std::uint8_t*>(&_unit);
    _serialized_data.insert(_serialized_data.end(), unit_bytes, unit_bytes + sizeof(Unit));
}

template <typename Channel>
void Message<Channel>::append_microseconds(const Microseconds& value) {
    auto raw_value = value.count();
    const std::uint8_t* value_bytes = reinterpret_cast<const std::uint8_t*>(&raw_value);
    _serialized_data.insert(_serialized_data.end(), value_bytes, value_bytes + sizeof(Microseconds::rep));
}

template <typename Channel>
void Message<Channel>::append_value() {
    _serialized_data.insert(_serialized_data.end(), _value.begin(), _value.end());
}

template <typename Channel>
typename Message<Channel>::Origin Message<Channel>::extract_origin(const std::uint8_t* data, unsigned int& offset, unsigned int size) {
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

template <typename Channel>
typename Message<Channel>::Type Message<Channel>::extract_type(const std::uint8_t* data, unsigned int& offset, unsigned int size) {
    if (offset + sizeof(std::uint8_t) > size)
        return Type::UNKNOWN;

    std::uint8_t raw_type = data[offset++];
    Type extracted_type = static_cast<Type>(raw_type);
    
    // Validate the extracted type to detect corruption
    if (extracted_type != Type::UNKNOWN && extracted_type != Type::INVALID &&
        extracted_type != Type::INTEREST && extracted_type != Type::RESPONSE &&
        extracted_type != Type::STATUS && extracted_type != Type::PTP &&
        extracted_type != Type::JOIN) {
        db<Message<Channel>>(ERR) << "Message::extract_type() detected corrupted type value: " 
                                  << static_cast<int>(raw_type) << " - marking as INVALID\n";
        return Type::INVALID;
    }
    
    return extracted_type;
}

template <typename Channel>
typename Message<Channel>::Unit Message<Channel>::extract_unit(const std::uint8_t* data, unsigned int& offset, unsigned int size) {
    unsigned int unit_size = sizeof(Unit);

    if (offset + unit_size > size)
        return Unit();

    Unit unit;
    std::memcpy(&unit, data + offset, unit_size);
    offset += unit_size;

    return unit;
}

template <typename Channel>
typename Message<Channel>::Microseconds Message<Channel>::extract_microseconds(const std::uint8_t* data, unsigned int& offset, unsigned int size) {
    unsigned int rep_size = sizeof(Microseconds::rep);

    if (offset + rep_size > size)
        return ZERO;

    Microseconds::rep raw_value;
    std::memcpy(&raw_value, data + offset, rep_size);
    offset += rep_size;

    return Microseconds(raw_value);
}

// Clock integration utility method implementations
template <typename Protocol>
typename Message<Protocol>::Microseconds Message<Protocol>::getSynchronizedTimestamp() {
    auto& clock = Clock::getInstance();
    bool sync;
    auto synchronized_time = clock.getSynchronizedTime(&sync);
    return Microseconds(std::chrono::duration_cast<std::chrono::microseconds>(synchronized_time.time_since_epoch()).count());
}

template <typename Protocol>
bool Message<Protocol>::isClockSynchronized() {
    auto& clock = Clock::getInstance();
    return clock.isFullySynchronized();
}

#endif // MESSAGE_H