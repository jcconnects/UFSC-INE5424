#ifndef COMMUNICATOR_H
#define COMMUNICATOR_H

#include <stdexcept>
#include <atomic>
#include <chrono>
#include <algorithm>

// Forward declare ComponentType with full definition to match component.h
enum class ComponentType : std::uint8_t {
    UNKNOWN = 0,
    GATEWAY,
    PRODUCER,
    CONSUMER,
    PRODUCER_CONSUMER // Dual role component
};

// Include other required headers
#include "teds.h"
#include "message.h"
#include "traits.h"
#include "debug.h"

// Forward declaration for circular dependency
class Component;

template <typename Channel>
class Communicator: public Concurrent_Observer<typename Channel::Observer::Observed_Data, typename Channel::Observer::Observing_Condition>
{
    
    public:
        typedef Concurrent_Observer<typename Channel::Observer::Observed_Data, typename Channel::Observer::Observing_Condition> Observer;
        typedef typename Channel::Buffer Buffer;
        typedef typename Channel::Address Address;
        typedef typename Channel::Port Port;
        
        static constexpr const unsigned int MAX_MESSAGE_SIZE = Channel::MTU; // Maximum message size in bytes
        static constexpr const Port GATEWAY_PORT = 0;
        static constexpr const Port INTERNAL_BROADCAST_PORT = 1;
        static constexpr const Port MIN_COMPONENT_PORT = 2;

        // Constructor and Destructor
        Communicator(Channel* channel, Address address, ComponentType owner_type = ComponentType::UNKNOWN, DataTypeId owner_data_type = DataTypeId::UNKNOWN);
        ~Communicator();
        
        // Message creation
        Message new_message(Message::Type message_type, DataTypeId unit_type, unsigned int period = 0, const void* value_data = nullptr, const unsigned int value_size = 0);

        // Communication methods
        bool send(const Message& message, const Address& destination = Channel::Address::BROADCAST);
        bool receive(Message* message);
        
        // Method to close the communicator and unblock any pending receive calls
        void close();

        // Check if communicator is closed
        const bool is_closed();

        // Address getter
        const typename Channel::Address& address() const;
        
        // Deleted copy constructor and assignment operator to prevent copying
        Communicator(const Communicator&) = delete;
        Communicator& operator=(const Communicator&) = delete;

        // Simplified interest management (one interest per consumer)
        bool set_interest(DataTypeId type, std::uint32_t period_us = 0);
        DataTypeId get_interest_type() const { return _interested_data_type; }
        std::uint32_t get_interest_period() const { return _interested_period_us; }
        
    private:
        using Observer::update;
        // Update method for Observer pattern
        void update(typename Channel::Observer::Observing_Condition c, typename Channel::Observer::Observed_Data* buf);

        Channel* _channel;
        Address _address;
        std::atomic<bool> _closed;
        
        // Simplified P3 attributes
        ComponentType _owner_type;
        DataTypeId _owner_data_type;             // For PRODUCER: the type produced
        DataTypeId _interested_data_type;        // For CONSUMER: the type interested in
        std::uint32_t _interested_period_us;     // For CONSUMER: the period interested in
        std::uint64_t _last_accepted_response_time_us; // For CONSUMER: used for period filtering
};

/*************** Communicator Implementation *****************/
template <typename Channel>
Communicator<Channel>::Communicator(Channel* channel, Address address, ComponentType owner_type, DataTypeId owner_data_type) 
    : Observer(address.port()), _channel(channel), _address(address), _closed(false), 
      _owner_type(owner_type), _owner_data_type(owner_data_type),
      _interested_data_type(DataTypeId::UNKNOWN), _interested_period_us(0), _last_accepted_response_time_us(0) {
    db<Communicator>(TRC) << "[Communicator] [" << _address.to_string() << "] Constructor called!\n";
    if (!channel) {
        throw std::invalid_argument("[Communicator] Channel pointer cannot be null");
    }

    // Attach to the channel with this communicator's address
    _channel->attach(this, address);
    
    // IMPORTANT: Also attach to INTERNAL_BROADCAST_PORT to receive relayed messages
    if (address.port() != INTERNAL_BROADCAST_PORT) {  // Avoid double registration for internal port
        Address broadcast_addr(address.paddr(), INTERNAL_BROADCAST_PORT);
        _channel->attach(this, broadcast_addr);
        db<Communicator>(INF) << "[Communicator] also attached to INTERNAL_BROADCAST_PORT (Port 1)\n";
    }
    
    db<Communicator>(INF) << "[Communicator] attached to Channel\n";
}

template <typename Channel>
Communicator<Channel>::~Communicator() {
    db<Communicator>(TRC) << "[Communicator] [" << _address.to_string() << "] Destructor called!\n";
    
    // Detach from channel
    _channel->detach(this, _address);
    db<Communicator>(INF) << "[Communicator] [" << _address.to_string() << "] detached from Channel!\n";
}

template <typename Channel>
Message Communicator<Channel>::new_message(Message::Type message_type, DataTypeId unit_type, unsigned int period, const void* value_data, const unsigned int value_size) {
    switch (message_type)
    {
        case Message::Type::INTEREST:
            return Message(message_type, _address, unit_type, period);
        case Message::Type::RESPONSE:
            return Message(message_type, _address, unit_type, 0, value_data, value_size);
        default:
            db<Communicator>(ERR) << "[Communicator] new_message() called with unknown or deprecated message type!\n";
            return Message(); // Return a default (invalid) message
    }
}

template <typename Channel>
bool Communicator<Channel>::send(const Message& message, const Address& destination) {
    db<Communicator>(TRC) << "[Communicator] [" << _address.to_string() << "] send() called!\n";
    
    // Check if communicator is closed before attempting to send
    if (is_closed()) {
        db<Communicator>(WRN) << "[Communicator] [" << _address.to_string() << "] send() called when communicator is closed! Returning False\n";
        return false;
    }
    
    if (message.size() == 0) {
        db<Communicator>(ERR) << "[Communicator] [" << _address.to_string()  << "] message is empty!\n";
        return false;
    }

    if (message.size() > MAX_MESSAGE_SIZE) {
        db<Communicator>(ERR) << "[Communicator] [" << _address.to_string() << "] message too big!\n";
        return false; 
    }
    
    try {
        // Use the provided destination address instead of BROADCAST
        int result = _channel->send(_address, destination, message.data(), message.size());
        db<Communicator>(INF) << "[Communicator] [" << _address.to_string() << "] Channel::send() message of size: " << std::to_string(result) << "\n";
        
        if (result <= 0) {
            db<Communicator>(ERR) << "[Communicator] [" << _address.to_string() << "] Failed to send message\n";
            return false;
        }
    } catch (const std::exception& e) {
        db<Communicator>(ERR) << "[Communicator] [" << _address.to_string() << "] Error sending message: " << e.what() << "\n";
        return false;
    }

    return true;
}

template <typename Channel>
bool Communicator<Channel>::receive(Message* message) {
    db<Communicator>(TRC) << "[Communicator] [" << _address.to_string() << "] receive() called!\n";
    
    // If communicator is closed, doesn't even try to receive
    if (is_closed()) {
        db<Communicator>(WRN) << "[Communicator] [" << _address.to_string() << "] receive() called while communicator is closed! Returning false\n";
        return false;
    }

    if (!message) {
        db<Communicator>(ERR) << "[Communicator] [" << _address.to_string() << "] Null message pointer in receive\n";
        return false;
    }
    
    Buffer* buf = Observer::updated(); // Blocks until a message is received
    db<Communicator>(INF) << "[Communicator] [" << _address.to_string() << "] buffer retrieved\n";

    if (!buf) {
        // Check if the communicator was closed while waiting
        if (is_closed()) {
            db<Communicator>(INF) << "[Communicator] [" << _address.to_string() << "] receive unblocked due to close(). Returning false.\n";
        } else {
             db<Communicator>(ERR) << "[Communicator] [" << _address.to_string() << "] received null buffer unexpectedly! Returning false.\n";
        }
        return false;
    }

    try {
        Address from; // Temporary to hold the source address from the channel
        std::uint8_t temp_data[MAX_MESSAGE_SIZE];

        // Pass MAX_MESSAGE_SIZE as the capacity of temp_data
        int size = _channel->receive(buf, &from, temp_data, MAX_MESSAGE_SIZE); 
        db<Communicator>(INF) << "[Communicator] Channel::receive() returned size of message: " << size << ".\n";

        if (size <= 0) {
            db<Communicator>(ERR) << "[Communicator] [" << _address.to_string() << "] failed to receive data.\n";
            _channel->free(buf); // Free buffer if channel receive failed
            return false;
        }

        // Deserialize the raw data into the message
        *message = Message::deserialize(temp_data, size);

        // Sets message origin address
        message->origin(from);
        db<Communicator>(INF) << "[Communicator] [" << _address.to_string() << "] Received message origin set to: " << from.to_string() << "\n";
        
        _channel->free(buf); // Free the buffer after successful processing
        return true;
    
    } catch (const std::exception& e) {
        db<Communicator>(ERR) << "[Communicator] [" << _address.to_string() << "] Error receiving message: " << e.what() << "\n";
        _channel->free(buf); // Free buffer on exception
        return false;
    }
}

template <typename Channel>
void Communicator<Channel>::close() {
    db<Communicator>(TRC) << "[Communicator] [" << _address.to_string() << "] close() called!\n";
    
    _closed.store(true, std::memory_order_release);
    
    // Signal any threads waiting on receive to wake up *without data*
    update(_address.port(), nullptr);
}

template <typename Channel>
const bool Communicator<Channel>::is_closed() { 
    return _closed.load(std::memory_order_acquire); 
}

template <typename Channel>
void Communicator<Channel>::update(typename Channel::Observer::Observing_Condition c, typename Channel::Observer::Observed_Data* buf) {
    db<Communicator>(TRC) << "[Communicator] [" << _address.to_string() << "] update() called with condition " << c << "!\n";
    
    // If buf is null, this is a shutdown signal, pass it through
    if (!buf) {
        Observer::update(c, buf);
        return;
    }
    
    try {
        // Define message field offsets and sizes as compile-time constants for peeking
        static constexpr std::size_t MSG_TYPE_OFFSET = 0;
        static constexpr std::size_t MSG_TYPE_SIZE = 1;
        // Origin Address: offset 1, size 8 - not directly used for this specific peek
        // Timestamp: offset 9, size 8 - not directly used for this specific peek
        static constexpr std::size_t UNIT_TYPE_OFFSET = MSG_TYPE_OFFSET + MSG_TYPE_SIZE + 8 /*Origin Size*/ + 8 /*Timestamp Size*/; // 0 + 1 + 8 + 8 = 17
        static constexpr std::size_t UNIT_TYPE_SIZE = 4;

        const std::size_t min_peek_size = UNIT_TYPE_OFFSET + UNIT_TYPE_SIZE; // 17 + 4 = 21

        if (buf->size() < min_peek_size) { 
            db<Communicator>(WRN) << "[Communicator] [" << _address.to_string() << "] Message too small for required header fields (need " << min_peek_size << "), passing through\n";
            Observer::update(c, buf);
            return;
        }
        
        std::uint8_t temp_peek_buffer[min_peek_size]; // Now min_peek_size is a compile-time constant
        _channel->peek(buf, temp_peek_buffer, min_peek_size);
        
        unsigned int current_msg_type_offset = MSG_TYPE_OFFSET;
        Message::Type msg_type = static_cast<Message::Type>(
            Message::extract_uint8t(temp_peek_buffer, current_msg_type_offset, min_peek_size)
        );
        
        unsigned int current_unit_type_offset = UNIT_TYPE_OFFSET;
        DataTypeId unit_type = static_cast<DataTypeId>(
            Message::extract_uint32t(temp_peek_buffer, current_unit_type_offset, min_peek_size)
        );

        // Apply simplified filtering logic based on component role
        bool should_deliver = false;
        
        // GATEWAY component - relays all messages
        if (_owner_type == ComponentType::GATEWAY) {
            should_deliver = true;
        }
        // PRODUCER component - receives INTEREST messages for its data type
        else if (_owner_type == ComponentType::PRODUCER && msg_type == Message::Type::INTEREST) {
            should_deliver = (unit_type == _owner_data_type);
        }
        // CONSUMER component - receives RESPONSE messages for its interested data type
        else if (_owner_type == ComponentType::CONSUMER && msg_type == Message::Type::RESPONSE) {
            if (unit_type == _interested_data_type) {
                // Apply period-based filtering if a period is set
                auto now = std::chrono::high_resolution_clock::now();
                auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                    now.time_since_epoch()).count();
                
                if (_interested_period_us == 0 || 
                   (now_us - _last_accepted_response_time_us >= _interested_period_us)) {
                    _last_accepted_response_time_us = now_us;
                    should_deliver = true;
                    db<Communicator>(INF) << "[Communicator] [" << _address.to_string() << "] RESPONSE message for type " 
                                         << static_cast<int>(unit_type) 
                                         << " passed period filter (period=" 
                                         << _interested_period_us << ")\n";
                } else {
                    db<Communicator>(INF) << "[Communicator] [" << _address.to_string() << "] RESPONSE message for type "
                                         << static_cast<int>(unit_type)
                                         << " filtered out due to period restriction\n";
                }
            }
        }
        // Unknown component type - handle the message but log a warning
        else if (_owner_type == ComponentType::UNKNOWN) {
            should_deliver = true;
            db<Communicator>(WRN) << "[Communicator] [" << _address.to_string() << "] Unknown component type, delivering message without filtering\n";
        }
        
        // Handle delivery or discard
        if (should_deliver) {
            Observer::update(c, buf);
            db<Communicator>(INF) << "[Communicator] [" << _address.to_string() << "] Message passed filter, delivered to component\n";
        } else {
            // Free the buffer if we're not delivering it
            _channel->free(buf);
            db<Communicator>(INF) << "[Communicator] [" << _address.to_string() << "] Message filtered out, not delivered to component\n";
        }
    } catch (const std::exception& e) {
        // Fall back to regular update behavior on error
        db<Communicator>(ERR) << "[Communicator] [" << _address.to_string() << "] Error during filtering: " << e.what() << ", passing through\n";
        Observer::update(c, buf);
    }
}

template <typename Channel>
const typename Communicator<Channel>::Address& Communicator<Channel>::address() const {
    return _address;
}

template <typename Channel>
bool Communicator<Channel>::set_interest(DataTypeId type, std::uint32_t period_us) {
    db<Communicator>(TRC) << "[Communicator] [" << _address.to_string() << "] set_interest() called for type " 
                         << static_cast<int>(type) << " with period " << period_us << "us\n";
    
    if (type == DataTypeId::UNKNOWN) {
        db<Communicator>(ERR) << "[Communicator] [" << _address.to_string() << "] Cannot set interest to UNKNOWN type\n";
        return false;
    }
    
    _interested_data_type = type;
    _interested_period_us = period_us;
    
    db<Communicator>(INF) << "[Communicator] [" << _address.to_string() << "] Interest set for type " 
                         << static_cast<int>(type) << " with period " << period_us << "us\n";
    return true;
}

#endif // COMMUNICATOR_H
