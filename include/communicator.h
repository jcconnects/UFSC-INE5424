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
        bool send(const Message<MAX_MESSAGE_SIZE>* message);
        bool receive(Message<MAX_MESSAGE_SIZE>* message);
        
        // Method to close the communicator and unblock any pending receive calls
        void close();
        
        // Method to reopen a previously closed communicator
        void reopen();
        
        // Check if communicator is closed
        bool is_closed() const { 
            return _closed.load(std::memory_order_acquire); 
        }
        
        // Deleted copy constructor and assignment operator to prevent copying
        Communicator(const Communicator&) = delete;
        Communicator& operator=(const Communicator&) = delete;

    private:
        // Update method for Observer pattern
        void update(typename Channel::Observed* obs, typename Channel::Observer::Observing_Condition c, Buffer* buf);

    private:
        Channel* _channel;
        Address _address;
        std::atomic<bool> _closed;
};

// Template implementations
template <typename Channel>
Communicator<Channel>::Communicator(Channel* channel, Address address) 
    : Observer(address.port()), 
      _channel(channel), 
      _address(address),
      _closed(false) {
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
    
    // Ensure communicator is closed before destructing
    close();
    
    // Then detach from channel
    if (_channel) {
        _channel->detach(this, _address);
        db<Communicator>(INF) << "[Communicator] detached from Channel\n";
    }
    
    db<Communicator>(INF) << "[Communicator] destroyed!\n";
}

template <typename Channel>
bool Communicator<Channel>::send(const Message<MAX_MESSAGE_SIZE>* message) {
    db<Communicator>(TRC) << "Communicator<Channel>::send() called!\n";
    
    // Check if communicator is closed before attempting to send
    if (is_closed()) {
        db<Communicator>(WRN) << "[Communicator] send() called while communicator is closed\n";
        return false;
    }
    
    if (!message) {
        std::cerr << "Error: Null message pointer in send" << std::endl;
        return false;
    }

    try {
        // Check if the channel is still active
        if (_channel && !is_closed()) {
            int result = _channel->send(_address, Address::BROADCAST, message->data(), message->size());
            db<Communicator>(INF) << "[Communicator] Channel::send() return value " << std::to_string(result) << "\n";

            if (result <= 0) {
                db<Communicator>(ERR) << "[Communicator] Failed to send message\n";
                return false;
            }

            return true;
        } else {
            db<Communicator>(WRN) << "[Communicator] Channel inactive or communicator closed during send\n";
            return false;
        }
    } catch (const std::exception& e) {
        db<Communicator>(ERR) << "[Communicator] Error sending message: " << e.what() << "\n";
        return false;
    }
}

template <typename Channel>
bool Communicator<Channel>::receive(Message<MAX_MESSAGE_SIZE>* message) {
    db<Communicator>(TRC) << "Communicator<Channel>::receive() called!\n";
    
    // If communicator is closed, doesn't even try to receive
    if (is_closed()) {
        db<Communicator>(INF) << "[Communicator] closed! Returning false\n";
        return false;
    }

    if (!message) {
        db<Communicator>(ERR) << "[Communicator] Null message pointer in receive\n";
        return false;
    }
    
    Buffer* buf = Observer::updated();
    db<Communicator>(INF) << "[Communicator] buffer retrieved\n";

    // Check if communicator was closed during wait
    if (is_closed()) {
        db<Communicator>(INF) << "[Communicator] closed during wait! Returning false\n";
        return false;
    }

    // Check for nullptr buffer which indicates a close signal
    if (!buf) {
        db<Communicator>(INF) << "[Communicator] received close signal (nullptr buffer)! Returning false\n";
        return false;
    }

    if (buf->size() == 0) {
        db<Communicator>(INF) << "[Communicator] empty buffer! Returning false\n";
        return false;
    }

    try {
        // Check again if communicator is still open and channel is valid
        if (!is_closed() && _channel) {
            Address from;
            std::uint8_t temp_data[MAX_MESSAGE_SIZE];

            int size = _channel->receive(buf, from, temp_data, buf->size());
            db<Communicator>(INF) << "[Communicator] Channel::receive() returned size " << std::to_string(size) << "\n";
            
            if (size > 0) {
                // Create a new message with the received data
                *message = Message<MAX_MESSAGE_SIZE>(temp_data, static_cast<unsigned int>(size));
                return true;
            }
        }
        
        return false;

    } catch (const std::exception& e) {
        db<Communicator>(ERR) << "[Communicator] Error receiving message: " << e.what() << "\n";
        return false;
    }
}

template <typename Channel>
void Communicator<Channel>::close() {
    db<Communicator>(TRC) << "Communicator<Channel>::close() called!\n";
    
    // Use atomic compare_exchange to ensure we only close once
    bool expected = false;
    if (!_closed.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        db<Communicator>(INF) << "[Communicator] Already closed, skipping\n";
        return;
    }
    
    try {
        db<Communicator>(INF) << "[Communicator] Unblocking any threads waiting on receive()\n";
        
        // Call update multiple times to ensure it propagates
        for (int i = 0; i < 5; i++) {
            // Pass nullptr instead of a potentially dangling local buffer
            update(nullptr, _address.port(), nullptr); // Signal with nullptr to indicate close
            usleep(1000); // Short sleep to allow thread scheduling
        }
        
        db<Communicator>(INF) << "[Communicator] Successfully closed\n";
    } catch (const std::exception& e) {
        db<Communicator>(ERR) << "[Communicator] Error during communicator close: " << e.what() << "\n";
    }
}

template <typename Channel>
void Communicator<Channel>::reopen() {
    db<Communicator>(TRC) << "Communicator<Channel>::reopen() called!\n";
    
    // Only attempt to reopen if currently closed
    bool expected = true;
    if (!_closed.compare_exchange_strong(expected, false, std::memory_order_acq_rel)) {
        db<Communicator>(INF) << "[Communicator] Already open, skipping reopen\n";
        return;
    }
    
    db<Communicator>(INF) << "[Communicator] Successfully reopened\n";
}

template <typename Channel>
void Communicator<Channel>::update(typename Channel::Observed* obs, typename Channel::Observer::Observing_Condition c, Buffer* buf) {
    db<Communicator>(TRC) << "Communicator<Channel>::update() called!\n";
    Observer::update(c, buf); // releases the thread waiting for data
}

#endif // COMMUNICATOR_H
