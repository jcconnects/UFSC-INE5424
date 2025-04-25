#ifndef COMMUNICATOR_H
#define COMMUNICATOR_H

#include <iostream>
#include <cstring> // For memcpy
#include <stdexcept>
#include <unistd.h> // For usleep
#include <atomic>

#include "protocol.h"
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
        
        // Constructor and Destructor
        Communicator(Channel* channel, Address address);
        ~Communicator();
        
        // Communication methods
        bool send(const Address& destination, const Message<MAX_MESSAGE_SIZE>* message);
        bool receive(Message<MAX_MESSAGE_SIZE>* message, Address* source_address);
        
        // Method to close the communicator and unblock any pending receive calls
        void close();

        
        // Check if communicator is closed
        bool is_closed() const { 
            return _closed.load(std::memory_order_acquire); 
        }
        
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
bool Communicator<Channel>::send(const Address& destination, const Message<MAX_MESSAGE_SIZE>* message) {
    db<Communicator>(TRC) << "Communicator<Channel>::send() called!\n";
    
    // Check if communicator is closed before attempting to send
    if (is_closed()) {
        db<Communicator>(WRN) << "[Communicator] send() called while communicator is closed! Returning False\n";
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
bool Communicator<Channel>::receive(Message<MAX_MESSAGE_SIZE>* message, Address* source_address) {
    db<Communicator>(TRC) << "Communicator<Channel>::receive() called!\n";
    
    // If communicator is closed, doesn't even try to receive
    if (is_closed()) {
        db<Communicator>(INF) << "[Communicator] receive() called while communicator is closed! Returning false\n";
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

    // Handle potential empty buffer signalling closure
    if (buf->size() == 0) {
        if (is_closed()) {
            delete buf;
            db<Communicator>(INF) << "[Communicator] empty buffer, but communicator was closed. Deleting buffer.\n";
        } else {
            db<Communicator>(WRN) << "[Communicator] received empty buffer unexpectedly! Returning false.\n";
        }
        return false;
    }

    try {
        Address from; // Temporary to hold the source address from the channel
        std::uint8_t temp_data[MAX_MESSAGE_SIZE];

        int size = _channel->receive(buf, from, temp_data, buf->size()); // Assuming Channel::receive fills 'from'
        db<Communicator>(INF) << "[Communicator] Channel::receive() returned size " << std::to_string(size) << "\n";
        
        if (size > 0) {
            // Create a new message with the received data
            *message = Message<MAX_MESSAGE_SIZE>(temp_data, static_cast<unsigned int>(size));
            
            // Populate the source_address output parameter if provided
            if (source_address) {
                *source_address = from;
            }
            return true;
        }

        // If size <= 0, receive failed or returned no data
        return false;
    
    } catch (const std::exception& e) {
        db<Communicator>(ERR) << "[Communicator] Error receiving message: " << e.what() << "\n";
        return false;
    }
}

template <typename Channel>
void Communicator<Channel>::close() {
    db<Communicator>(TRC) << "Communicator<Channel>::close() called!\n";
    
    _closed.store(true, std::memory_order_release);

    try {
        // Signal any threads waiting on receive to wake up
        Buffer* buf = new Buffer();
        update(nullptr, _address.port(), buf);
    } catch (const std::exception& e) {
        std::cerr << "Error during communicator close: " << e.what() << std::endl;
    }
}

template <typename Channel>
void Communicator<Channel>::update(typename Channel::Observed* obs, typename Channel::Observer::Observing_Condition c, Buffer* buf) {
    db<Communicator>(TRC) << "Communicator<Channel>::update() called!\n";
    Observer::update(c, buf); // releases the thread waiting for data
}

#endif // COMMUNICATOR_H
