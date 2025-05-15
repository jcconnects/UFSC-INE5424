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

        bool add_interest(DataTypeId type, std::uint64_t period_us = 0);
        bool remove_interest(DataTypeId type);
        void clear_interests() { _interests.clear(); }
        
    protected:
        struct Interest {
            DataTypeId type;
            std::uint64_t last_accepted_response_time_us = 0;
            std::uint64_t period_us = 0;
        };
        
    private:
        using Observer::update;
        // Update method for Observer pattern
        void update(typename Channel::Observer::Observing_Condition c, typename Channel::Observer::Observed_Data* buf);

        Channel* _channel;
        Address _address;
        std::atomic<bool> _closed;
        // Owner component reference for P3 filtering
        ComponentType _owner_type;
        DataTypeId _owner_data_type;
        std::vector<Interest> _interests; // List of interests for filtering

};

/*************** Communicator Implementation *****************/
template <typename Channel>
Communicator<Channel>::Communicator(Channel* channel, Address address, ComponentType owner_type, DataTypeId owner_data_type) 
    : Observer(address.port()), _channel(channel), _address(address), _closed(false), 
      _owner_type(owner_type), _owner_data_type(owner_data_type) {
    db<Communicator>(TRC) << "[Communicator] [" << _address.to_string() << "] Constructor called!\n";
    if (!channel) {
        throw std::invalid_argument("[Communicator] Channel pointer cannot be null");
    }

    _channel->attach(this, address);
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

        int size = _channel->receive(buf, &from, temp_data, buf->size()); // Assuming Channel::receive fills 'from'
        db<Communicator>(INF) << "[Communicator] Channel::receive() returned size of message: " << size << ".\n";

        if (size <= 0) {
            db<Communicator>(ERR) << "[Communicator] [" << _address.to_string() << "] failed to receive data.\n";
            return false;
        }

        // Deserialize the raw data into the message
        *message = Message::deserialize(temp_data, size);

        // Sets message origin address
        message->origin(from);
        db<Communicator>(INF) << "[Communicator] [" << _address.to_string() << "] Received message origin set to: " << from << "\n";

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
    db<Communicator>(TRC) << "[Communicator] [" << _address.to_string() << "] update() called!\n";
    
    // If buf is null or we have no owner component, proceed with standard update
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
        
        // Apply filtering logic based on the component's role and message type
        bool should_deliver = false;
        
        
        switch (_owner_type) {
            case ComponentType::GATEWAY:
                // Gateway relays INTEREST and RESPONSE messages
                should_deliver = (msg_type == Message::Type::INTEREST || 
                                 msg_type == Message::Type::RESPONSE);
                break;
            
            case ComponentType::PRODUCER_CONSUMER:
            case ComponentType::PRODUCER:
                // Producer components care about INTEREST messages for their data type
                if (msg_type == Message::Type::INTEREST) {
                    should_deliver = (unit_type == _owner_data_type);
                }
                
                // If it's a PRODUCER_CONSUMER, fall through to also check CONSUMER logic
                if (_owner_type == ComponentType::PRODUCER) {
                    break;
                }
                // Fall through if PRODUCER_CONSUMER
                
            case ComponentType::CONSUMER:
                // Consumer components care about RESPONSE messages for their interests
                if (msg_type == Message::Type::RESPONSE && !should_deliver) {
                    // Apply period-based filtering
                    auto now = std::chrono::high_resolution_clock::now();
                    auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                        now.time_since_epoch()).count();
                    
                    // Iterate through active interests
                    for (auto& interest : _interests) {
                        if (interest.type == unit_type) {
                            if (interest.period_us == 0 || // No filtering if period is 0
                               (now_us - interest.last_accepted_response_time_us >= interest.period_us)) {
                                interest.last_accepted_response_time_us = now_us;
                                should_deliver = true;
                                db<Communicator>(INF) << "[Communicator] [" << _address.to_string() << "] RESPONSE message for type " 
                                                    << static_cast<int>(unit_type) 
                                                    << " passed period filter (period=" 
                                                    << interest.period_us << ")\n";
                                break;
                            } else {
                                db<Communicator>(INF) << "[Communicator] [" << _address.to_string() << "] RESPONSE message for type "
                                                    << static_cast<int>(unit_type)
                                                    << " filtered out due to period restriction\n";
                            }
                        }
                    }
                }
                break;
                
            default:
                // For UNKNOWN component type, don't filter
                should_deliver = true;
                break;
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
bool Communicator<Channel>::add_interest(DataTypeId type, std::uint64_t period_us) {
    db<Communicator>(TRC) << "[Communicator] [" << _address.to_string() << "] add_interest() called!\n";
    
    // Check if the interest already exists
    for (const auto& interest : _interests) {
        if (interest.type == type) {
            db<Communicator>(WRN) << "[Communicator] [" << _address.to_string() << "] Interest already exists for type " << static_cast<int>(type) << "\n";
            return false;
        }
    }
    
    // Add new interest with the specified period
    _interests.push_back({type, 0, period_us});
    db<Communicator>(INF) << "[Communicator] [" << _address.to_string() << "] Interest added for type " << static_cast<int>(type) << " with period " << period_us << " microseconds\n";
    return true;
}

template <typename Channel>
bool Communicator<Channel>::remove_interest(DataTypeId type) {
    db<Communicator>(TRC) << "[Communicator] [" << _address.to_string() << "] remove_interest() called!\n";
    
    // Find and remove the interest
    auto it = std::remove_if(_interests.begin(), _interests.end(), 
                             [type](const Interest& interest) { return interest.type == type; });
    
    if (it != _interests.end()) {
        _interests.erase(it, _interests.end());
        db<Communicator>(INF) << "[Communicator] [" << _address.to_string() << "] Interest removed for type " << static_cast<int>(type) << "\n";
        return true;
    }
    
    db<Communicator>(WRN) << "[Communicator] [" << _address.to_string() << "] No interest found for type " << static_cast<int>(type) << "\n";
    return false;
}

#endif // COMMUNICATOR_H
