#ifndef COMMUNICATOR_H
#define COMMUNICATOR_H

#include <stdexcept>
#include <atomic>

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
        
        class Message {
            friend class Communicator<Channel>;

            private:
                // Constructors
                Message(const void* data, unsigned int size, const Address& origin);
                
            public:
                Message();
                ~Message() = default;
            
                // Getters
                const void* data() const; // Returns const void* for generic access
                const unsigned int size() const;
                
                // Setters
                void setData(const void* data, unsigned int size);

                // Origin Address getter/setter
                const Address& origin() const;
                void origin(const Address& addr);

            private:
                std::uint8_t _data[MAX_MESSAGE_SIZE];
                unsigned int _size;
                Address _origin;
        };

        // Constructor and Destructor
        Communicator(Channel* channel, Address address);
        ~Communicator();
        
        // Message creation
        Message* new_message(const void* data, unsigned int size);

        // Communication methods
        bool send(const Message* message, const Address& destination = Channel::Address::BROADCAST);
        bool receive(Message* message);
        
        // Method to close the communicator and unblock any pending receive calls
        void close();

        // Check if communicator is closed
        const bool is_closed();
        
        // Deleted copy constructor and assignment operator to prevent copying
        Communicator(const Communicator&) = delete;
        Communicator& operator=(const Communicator&) = delete;

    private:

        using Observer::update;
        // Update method for Observer pattern
        void update(typename Channel::Observed* obs, typename Channel::Observer::Observing_Condition c, Buffer* buf);

    private:
        Channel* _channel;
        Address _address;
        std::atomic<bool> _closed;
};

// Add the definition for the static constexpr member
template <typename Channel>
constexpr const unsigned int Communicator<Channel>::MAX_MESSAGE_SIZE;

/*************** Message Implementation ******************/
template <typename Channel>
Communicator<Channel>::Message::Message() :_size(0), _origin() {
    std::memset(_data, 0, MAX_MESSAGE_SIZE);
}

template<typename Channel>
Communicator<Channel>::Message::Message(const void* data, unsigned int size, const Address& origin) : _size(size), _origin(origin) {
    std::memcpy(_data, data, _size);
}

template<typename Channel>
const void* Communicator<Channel>::Message::data() const{
    return static_cast<const void*>(_data);
}

template <typename Channel>
const unsigned int Communicator<Channel>::Message::size() const {
    return _size;
}

template <typename Channel>
void Communicator<Channel>::Message::setData(const void* data, unsigned int size) {
    _size = size;
    std::memcpy(_data, data, _size);
}

template <typename Channel>
const typename Communicator<Channel>::Address& Communicator<Channel>::Message::origin() const {
    return _origin;
}

template <typename Channel>
void Communicator<Channel>::Message::origin(const Address& addr) {
    _origin = addr;
}

/*************** Communicator Implementation *****************/
template <typename Channel>
Communicator<Channel>::Communicator(Channel* channel, Address address) : Observer(address.port()), _channel(channel), _address(address), _closed(false) {
    db<Communicator>(TRC) << "Communicator<Channel>::Communicator() called!\n";
    if (!channel) {
        throw std::invalid_argument("Channel pointer cannot be null");
    }

    _channel->attach(this, address);
    db<Communicator>(INF) << "[Communicator] attached to Channel\n";
}

template <typename Channel>
Communicator<Channel>::~Communicator() {
    db<Communicator>(TRC) << "Communicator<Channel>::~Communicator() called!\n";
    
    // Detach from channel
    _channel->detach(this, _address);
    db<Communicator>(INF) << "[Communicator] detached from Channel\n";
    
    db<Communicator>(INF) << "[Communicator] destroyed!\n";
}

template <typename Channel>
typename Communicator<Channel>::Message* Communicator<Channel>::new_message(const void* data, unsigned int size) {
    if (size > MAX_MESSAGE_SIZE) {
        db<Communicator>(WRN) << "[Communicator] attempted creating message with oversize.\n";
        return nullptr;
    }

    if (size == 0) {
        return new Message();
    }

    if (data == nullptr) {
        db<Communicator>(WRN) << "[Communicator] attempted creating message with null data.\n";
        return nullptr;
    }

    return new Message(data, size, _address);
}

template <typename Channel>
bool Communicator<Channel>::send(const Message* message, const Address& destination) {
    db<Communicator>(TRC) << "Communicator<Channel>::send() called!\n";
    
    // Check if communicator is closed before attempting to send
    if (is_closed()) {
        db<Communicator>(WRN) << "[Communicator] send() called when communicator is closed! Returning False\n";
        return false;
    }
    
    if (!message) {
        db<Communicator>(ERR) << "[Communicator] Null message pointer in send\n";
        return false;
    }
    
    try {
        // Use the provided destination address instead of BROADCAST
        int result = _channel->send(_address, destination, message->data(), message->size());
        db<Communicator>(INF) << "[Communicator] Channel::send() return value " << std::to_string(result) << "\n";
        
        if (result <= 0) {
            db<Communicator>(ERR) << "[Communicator] Failed to send message\n";
            return false;
        }
    } catch (const std::exception& e) {
        db<Communicator>(ERR) << "[Communicator] Error sending message: " << e.what() << "\n";
        return false;
    }

    return true;
}

template <typename Channel>
bool Communicator<Channel>::receive(Message* message) {
    db<Communicator>(TRC) << "Communicator<Channel>::receive() called!\n";
    
    // If communicator is closed, doesn't even try to receive
    if (is_closed()) {
        db<Communicator>(WRN) << "[Communicator] receive() called while communicator is closed! Returning false\n";
        return false;
    }

    if (!message) {
        db<Communicator>(ERR) << "[Communicator] Null message pointer in receive\n";
        return false;
    }
    
    Buffer* buf = Observer::updated(); // Blocks until a message is received
    db<Communicator>(INF) << "[Communicator] buffer retrieved\n";

    if (!buf) {
        // Check if the communicator was closed while waiting
        if (is_closed()) {
            db<Communicator>(INF) << "[Communicator] receive unblocked due to close(). Returning false.\n";
        } else {
             db<Communicator>(ERR) << "[Communicator] received null buffer unexpectedly! Returning false.\n";
        }
        return false;
    }

    try {
        Address from; // Temporary to hold the source address from the channel
        std::uint8_t temp_data[MAX_MESSAGE_SIZE];

        int size = _channel->receive(buf, &from, temp_data, buf->size()); // Assuming Channel::receive fills 'from'
        
        if (size <= 0) {
            db<Communicator>(ERR) << "[Communicator] failed to receive data.\n";
            return false;
        }

        // Sets message content
        message->setData(static_cast<void*>(temp_data), size);

        // --- Populate Origin Address --- 
        message->origin(from);
        db<Communicator>(INF) << "[Communicator] Received message origin set to: " << from << "\n";
        // -----------------------------

        return true;
    
    } catch (const std::exception& e) {
        db<Communicator>(ERR) << "[Communicator] Error receiving message: " << e.what() << "\n";
        _channel->free(buf); // Free buffer on exception
        return false;
    }
}

template <typename Channel>
void Communicator<Channel>::close() {
    db<Communicator>(TRC) << "Communicator<Channel>::close() called!\n";
    
    _closed.store(true, std::memory_order_release);
    
    // Signal any threads waiting on receive to wake up *without data*
    update(nullptr, _address.port(), nullptr);
}

template <typename Channel>
const bool Communicator<Channel>::is_closed() { 
    return _closed.load(std::memory_order_acquire); 
}

template <typename Channel>
void Communicator<Channel>::update(typename Channel::Observed* obs, typename Channel::Observer::Observing_Condition c, Buffer* buf) {
    db<Communicator>(TRC) << "Communicator<Channel>::update() called!\n";
    Observer::update(c, buf); // releases the thread waiting for data
}

#endif // COMMUNICATOR_H
