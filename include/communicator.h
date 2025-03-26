#ifndef COMMUNICATOR_H
#define COMMUNICATOR_H

#include "observer.h"
#include "stubs/communicator_stubs.h" // Include actual Message implementation
#include <iostream>
#include <cstring> // For memcpy

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

    Communicator(Channel* channel, Address address)
        : Observer(address._port), _channel(channel), _address(address) {
        _channel->attach(this, address);
    }

    ~Communicator() {
        _channel->detach(this, _address);
    }

    bool send(const Message* message) {
        if (!message) return false;
        
        return (_channel->send(_address, Address::BROADCAST, message->data(), message->size()) > 0);
    }

    bool receive(Message* message) {
        if (!message) return false;
        
        Buffer* buf = Observer::updated(); // Block until a notification is triggered
        
        if (!buf) return false;
        
        Address from;
        
        // Create a temporary buffer to receive data
        char temp_buffer[1024] = {0}; // Assuming messages are smaller than 1024 bytes
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
    }

private:
    Channel* _channel;
    Address _address;
};

#endif // COMMUNICATOR_H
