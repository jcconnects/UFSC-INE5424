#ifndef COMMUNICATOR_H
#define COMMUNICATOR_H

#include "observer.h"
#include "stubs/communicator_stubs.h" // Include actual Message implementation
#include <iostream>
#include <cstring> // For memcpy
#include <mutex>
#include <stdexcept>

// Forward declarations
class Message;

template <typename Channel>
class Communicator: public Concurrent_Observer<typename Channel::Buffer, typename Channel::Port>
{
public:
    typedef typename Channel::Buffer Buffer;
    typedef typename Channel::Address Address;
    typedef typename Channel::Port Port;
    typedef Concurrent_Observer<Buffer, Port> Observer;
    
    static constexpr size_t MAX_MESSAGE_SIZE = 1024; // Maximum message size in bytes
    
    // Constructor and Destructor
    Communicator(Channel* channel, Address address);
    ~Communicator();
    
    // Communication methods
    bool send(const Message* message);
    bool receive(Message* message);
    
    // Deleted copy constructor and assignment operator to prevent copying
    Communicator(const Communicator&) = delete;
    Communicator& operator=(const Communicator&) = delete;

private:
    Channel* _channel;
    Address _address;
    mutable std::mutex _mutex;
};

// Template implementations
template <typename Channel>
Communicator<Channel>::Communicator(Channel* channel, Address address)
    : Observer(address._port), _channel(channel), _address(address) {
    if (!channel) {
        throw std::invalid_argument("Channel pointer cannot be null");
    }
    std::lock_guard<std::mutex> lock(_mutex);
    _channel->attach(this, address);
}

template <typename Channel>
Communicator<Channel>::~Communicator() {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_channel) {
        _channel->detach(this, _address);
    }
}

template <typename Channel>
bool Communicator<Channel>::send(const Message* message) {
    if (!message) {
        std::cerr << "Error: Null message pointer in send" << std::endl;
        return false;
    }
    
    if (message->size() > MAX_MESSAGE_SIZE) {
        std::cerr << "Error: Message size exceeds maximum allowed size" << std::endl;
        return false;
    }
    
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_channel) {
        std::cerr << "Error: Channel not initialized" << std::endl;
        return false;
    }
    
    try {
        int result = _channel->send(_address, Address::BROADCAST, message->data(), message->size());
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
bool Communicator<Channel>::receive(Message* message) {
    if (!message) {
        std::cerr << "Error: Null message pointer in receive" << std::endl;
        return false;
    }
    
    Buffer* buf = nullptr;
    try {
        buf = Observer::updated(); // Block until a notification is triggered
        if (!buf) {
            std::cerr << "Error: Null buffer received" << std::endl;
            return false;
        }
        
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_channel) {
            std::cerr << "Error: Channel not initialized" << std::endl;
            return false;
        }
        
        Address from;
        char temp_buffer[MAX_MESSAGE_SIZE] = {0};
        int size = _channel->receive(buf, &from, temp_buffer, sizeof(temp_buffer));
        
        if (size > 0) {
            // Create a new message with the received data
            std::string received_content(temp_buffer, size);
            *message = Message(received_content);
        }
        
        // Decrement reference count for buffer after use
        if (buf->ref_count.fetch_sub(1) == 1) {
            delete buf;
        }
        
        return (size > 0);
    } catch (const std::exception& e) {
        std::cerr << "Error receiving message: " << e.what() << std::endl;
        // Clean up buffer if an exception occurred
        if (buf && buf->ref_count.fetch_sub(1) == 1) {
            delete buf;
        }
        return false;
    }
}

#endif // COMMUNICATOR_H
