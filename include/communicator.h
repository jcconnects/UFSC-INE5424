#ifndef COMMUNICATOR_H
#define COMMUNICATOR_H

#include <stdexcept>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <functional>

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

        // Define callback type for handling interest periods
        typedef std::function<void(const Message& interest_msg)> InterestPeriodCallback;

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
        
        // Set callback for handling interest periods - called by Component during initialization
        void set_interest_period_callback(InterestPeriodCallback callback) {
            _interest_period_callback = callback;
        }
        
        // Set callback for handling responses - called by Component during initialization
        typedef std::function<void(const Message& response_msg)> ResponseHandlerCallback;
        void set_response_handler_callback(ResponseHandlerCallback callback) {
            _response_handler_callback = callback;
        }
        
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
        uint32_t _interested_period_us;     // For CONSUMER: the period interested in
        
        // Callback for handling interest periods
        InterestPeriodCallback _interest_period_callback;

        // Callback for handling responses - set by Component during initialization
        std::function<void(const Message& response_msg)> _response_handler_callback;
};

/*************** Communicator Implementation *****************/
template <typename Channel>
Communicator<Channel>::Communicator(Channel* channel, Address address, ComponentType owner_type, DataTypeId owner_data_type) 
    : Observer(address.port()), _channel(channel), _address(address), _closed(false),
     _owner_type(owner_type), _owner_data_type(owner_data_type),
      _interested_data_type(DataTypeId::UNKNOWN), _interested_period_us(0) {
    db<Communicator>(TRC) << "[Communicator] [" << _address.to_string() << "] Constructor called!\n";
    if (!channel) {
        throw std::invalid_argument("[Communicator] Channel pointer cannot be null");
    }

    // Attach to the channel with this communicator's address (its own specific port)
    _channel->attach(this, address);
    
    // REMOVED: Attaching to INTERNAL_BROADCAST_PORT for all components.
    // Components will now receive directly on their own port from the Gateway.
    
    db<Communicator>(INF) << "[Communicator] [" << _address.to_string() << "] attached to Channel on port " << address.port() << "\n";
}

template <typename Channel>
Communicator<Channel>::~Communicator() {
    db<Communicator>(TRC) << "[Communicator] [" << _address.to_string() << "] Destructor called!\n";
    
    // Detach from channel (from the component's own specific port)
    _channel->detach(this, _address);
    // REMOVED: Detaching from INTERNAL_BROADCAST_PORT.
    
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
        db<Communicator>(INF) << "[Communicator] [" << _address.to_string() << "] Received message from: " << from.to_string() << "\n";
        
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

template <typename Channel>
void Communicator<Channel>::update(typename Channel::Observer::Observing_Condition c, typename Channel::Observer::Observed_Data* buf) {
    db<Communicator>(TRC) << "[Communicator] [" << _address.to_string() << "] update() called with condition " << c << " (port)!\n";
    
    if (!buf) {
        Observer::update(c, buf); // Pass null buffer for shutdown/unblocking logic
        db<Communicator>(TRC) << "[Communicator] [" << _address.to_string() << "] update() called with null buffer, passing to Observer.\n";
        return;
    }
    
    try {
        // CHANGE: Add more safety checks before accessing buffer data
        if (!buf->data()) {
            db<Communicator>(ERR) << "[Communicator] [" << _address.to_string() << "] update() called with buffer that has null data.\n";
            _channel->free(buf);
            return;
        }
        
        Ethernet::Frame* eth_frame = reinterpret_cast<Ethernet::Frame*>(buf->data());
        // Validate protocol packet size before accessing it
        if (buf->size() <= Ethernet::HEADER_SIZE) {
            db<Communicator>(ERR) << "[Communicator] [" << _address.to_string() << "] update() called with buffer that has insufficient size.\n";
            _channel->free(buf);
            return;
        }
        
        Protocol<Vehicle::VehicleNIC>::Packet* proto_packet = 
            reinterpret_cast<Protocol<Vehicle::VehicleNIC>::Packet*>(eth_frame->payload);
            
        // Validate protocol packet size before deserializing
        if (proto_packet->size() == 0 || proto_packet->size() > Channel::MTU - sizeof(typename Protocol<Vehicle::VehicleNIC>::Header)) {
            db<Communicator>(ERR) << "[Communicator] [" << _address.to_string() << "] update() called with invalid protocol packet size: " << proto_packet->size() << "\n";
            _channel->free(buf);
            return;
        }
        
        Message full_msg = Message::deserialize(reinterpret_cast<const uint8_t*>(proto_packet->template data<void>()), proto_packet->size());

        // 1. GATEWAY on its listening port (GATEWAY_PORT = 0)
        if (_owner_type == ComponentType::GATEWAY && c == GATEWAY_PORT) {
            db<Communicator>(INF) << "[Communicator] [" << _address.to_string() << "] Gateway on Port " << c << " received msg type " 
                                 << static_cast<int>(full_msg.message_type()) << ". Passing to component's receive() queue.\n";
            Observer::update(c, buf); // Pass buffer to Gateway's receive() for its processing
            return; 
        }

        // 2. PRODUCER on its own component port (c == _address.port())
        if (_owner_type == ComponentType::PRODUCER && c == _address.port()) {
            if (full_msg.message_type() == Message::Type::INTEREST && full_msg.unit_type() == _owner_data_type) {
                if (_interest_period_callback) {
                    _interest_period_callback(full_msg);
                    db<Communicator>(INF) << "[Communicator] [" << _address.to_string() << "] Producer on its port " << c << " processed INTEREST for type " 
                                         << static_cast<int>(full_msg.unit_type()) << " period " << full_msg.period() << "us from " << full_msg.origin().to_string() << ". Invoked callback.\n";
                } else {
                    db<Communicator>(ERR) << "[Communicator] [" << _address.to_string() << "] Interest period callback not set for producer on port " << c << ".\n";
                }
                _channel->free(buf); // Message processed or callback missing, free buffer
            } else {
                _channel->free(buf); // Not a relevant INTEREST for this producer on its own port
                db<Communicator>(TRC) << "[Communicator] [" << _address.to_string() << "] Producer on its port " << c << " filtered out msg type " 
                                     << static_cast<int>(full_msg.message_type()) << " unit_type " 
                                     << static_cast<int>(full_msg.unit_type()) << " (expected INTEREST for " << static_cast<int>(_owner_data_type) << ").\n";
            }
            return; 
        }

        // 3. CONSUMER on its own component port (c == _address.port())
        if (_owner_type == ComponentType::CONSUMER && c == _address.port()) {
            if (full_msg.message_type() == Message::Type::RESPONSE && full_msg.unit_type() == _interested_data_type) {
                if (_response_handler_callback) {
                    db<Communicator>(INF) << "[Communicator] [" << _address.to_string() << "] Consumer on its port " << c << " received valid RESPONSE for type " 
                                          << static_cast<int>(full_msg.unit_type()) << ". Invoking callback.\n";
                    _response_handler_callback(full_msg); // Pass the full message
                }
                _channel->free(buf); 
                return; // Message handled (or attempted to be handled by callback)
            } else {
                _channel->free(buf); // Not a relevant RESPONSE for this consumer on its own port
                db<Communicator>(TRC) << "[Communicator] [" << _address.to_string() << "] Consumer on port " << c << " received non-matching RESPONSE or other msg type. Discarding.\n";
                return; 
            }
        }

        // 4. Message on an unexpected port for this component's role, or if component is not Gateway, Producer, or Consumer.
        //    If c == _address.port() but it's not G/P/C, it might be a generic component.
        //    If c != _address.port() and not Gateway, it's an unexpected port.
        if (c == _address.port()) { // Message on component's own port, but not covered by specific G/P/C logic above.
             db<Communicator>(INF) << "[Communicator] [" << _address.to_string() << "] Message on component's own port " << c 
                                  << " (type " << static_cast<int>(full_msg.message_type()) 
                                  << ", unit " << static_cast<int>(full_msg.unit_type()) 
                                  << "). Passing to component's general receive() queue (default path for type " << static_cast<int>(_owner_type) << ").\n";
             Observer::update(c, buf); // Standard observer behavior for its own port.
             return;
        }
        
        // 5. Default: If not processed by any rule above (e.g., message on an unexpected port for this component type)
        db<Communicator>(WRN) << "[Communicator] [" << _address.to_string() << "] Message on port " << c 
                             << " (type " << static_cast<int>(full_msg.message_type()) 
                             << ", unit " << static_cast<int>(full_msg.unit_type()) 
                             << ", component port " << _address.port() << ") not processed by specific P3 rules or for component's own port. Freeing buffer.\n";
        _channel->free(buf);

    } catch (const std::exception& e) {
        db<Communicator>(ERR) << "[Communicator] [" << _address.to_string() << "] Exception in update(): " << e.what() << ". Freeing buffer.\n";
        if (buf) { // Ensure buffer is freed if an exception occurs after it might have been processed partially
            _channel->free(buf); 
        }
        // Do not call Observer::update(c, buf) on exception as buffer processing failed or buffer is now freed.
    }
}

#endif // COMMUNICATOR_H
