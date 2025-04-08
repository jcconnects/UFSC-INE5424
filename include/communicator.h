#ifndef COMMUNICATOR_H
#define COMMUNICATOR_H

#include <iostream>
#include <cstring> // For memcpy
#include <stdexcept>
#include <unistd.h> // For usleep

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
        bool send(const Message<MAX_MESSAGE_SIZE>* message);
        bool receive(Message<MAX_MESSAGE_SIZE>* message);
        
        // Method to close the communicator and unblock any pending receive calls
        void close();
        
        // Deleted copy constructor and assignment operator to prevent copying
        Communicator(const Communicator&) = delete;
        Communicator& operator=(const Communicator&) = delete;

    private:
        // Update method for Observer pattern
        void update(typename Channel::Observed* obs, typename Channel::Observer::Observing_Condition c, Buffer* buf);

    private:
        Channel* _channel;
        Address _address;
};

// Template implementations
template <typename Channel>
Communicator<Channel>::Communicator(Channel* channel, Address address) : Observer(address.port()), _channel(channel), _address(address) {
    db<Communicator<Channel>>(TRC) << "Communicator<Channel>::Communicator() called!\n";
    if (!channel) {
        throw std::invalid_argument("Channel pointer cannot be null");
    }

    _channel->attach(this, address);
    db<Communicator<Channel>>(INF) << "[Communicator] attached to Channel\n";
}

template <typename Channel>
Communicator<Channel>::~Communicator() {
    db<Communicator<Channel>>(TRC) << "Communicator<Channel>::~Communicator() called!\n";
    if (_channel) {
        _channel->detach(this, _address);
        db<Communicator<Channel>>(INF) << "[Communicator] detached from Channel\n";
    }
}

template <typename Channel>
bool Communicator<Channel>::send(const Message<MAX_MESSAGE_SIZE>* message) {
    db<Communicator<Channel>>(TRC) << "Communicator<Channel>::send() called!\n";
    if (!message) {
        std::cerr << "Error: Null message pointer in send" << std::endl;
        return false;
    }

    try {
        int result = _channel->send(_address, Address::BROADCAST, message->data(), message->size());
        db<Communicator<Channel>>(INF) << "[Communicator] Channel::send() return value " << std::to_string(result) << "\n";

        if (result <= 0) {
            std::cerr << "Error: Failed to send message" << std::endl;
            return false;
        }

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error sending message: " << e.what() << std::endl;
        return false;
    }
}

template <typename Channel>
bool Communicator<Channel>::receive(Message<MAX_MESSAGE_SIZE>* message) {
    db<Communicator<Channel>>(TRC) << "Communicator<Channel>::receive() called!\n";
    
    if (!message) {
        std::cerr << "Error: Null message pointer in receive" << std::endl;
        return false;
    }
    
    Buffer* buf = Observer::updated();
    db<Communicator<Channel>>(INF) << "[Communicator] buffer retrieved\n";

    if (!buf) {
        // Check if this is likely a shutdown (observer has been detached) or a real error
        if (!_channel) {
            // During normal shutdown, we'll get a null buffer from the notify() call after detaching
            db<Communicator<Channel>>(TRC) << "[Communicator] null buffer received during shutdown (expected)\n";
        } else {
            // This is unexpected during normal operation and should be logged as an error
            std::cerr << "Error: Unexpected null buffer received during normal operation" << std::endl;
        }
        return false;
    }

    try {
        
        Address from;
        std::uint8_t temp_data[MAX_MESSAGE_SIZE];

        int size = _channel->receive(buf, from, temp_data, buf->size());
        db<Communicator<Channel>>(INF) << "[Communicator] Channel::receive() returned size " << std::to_string(size) << "\n";
        
        if (size > 0) {
            // Create a new message with the received data
            *message = Message<MAX_MESSAGE_SIZE>(temp_data, static_cast<unsigned int>(size));
            return true;
        }

        return false;

    } catch (const std::exception& e) {
        std::cerr << "Error receiving message: " << e.what() << std::endl;
        return false;
    }
}

template <typename Channel>
void Communicator<Channel>::close() {
    db<Communicator<Channel>>(TRC) << "Communicator<Channel>::close() called!\n";
    
    try {
        // First try to detach and reattach to force connection close
        if (_channel) {
            _channel->detach(this, _address);
            db<Communicator<Channel>>(INF) << "[Communicator] detached from Channel during close\n";
            
            // Wait a small amount of time to ensure connections are closed
            usleep(50000); // 50ms
            
            // Signal any threads waiting on receive to wake up
            Observer::notify();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error during communicator close: " << e.what() << std::endl;
    }
}

template <typename Channel>
void Communicator<Channel>::update(typename Channel::Observed* obs, typename Channel::Observer::Observing_Condition c, Buffer* buf) {
    db<Communicator<Channel>>(TRC) << "Communicator<Channel>::update() called!\n";
    Observer::update(c, buf); // releases the thread waiting for data
}

#endif // COMMUNICATOR_H
