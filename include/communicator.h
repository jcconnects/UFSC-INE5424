#ifndef COMMUNICATOR_H
#define COMMUNICATOR_H

#include "observer.h"
#include "protocol.h"

// Forward declaration for Message class
class Message {
public:
    virtual ~Message() {}
    virtual const void* data() const = 0;
    virtual unsigned int size() const = 0;
};

// Communication End-Point (for client classes)
template <typename Channel>
class Communicator: public Concurrent_Observer<typename Channel::Observer::Observed_Data,
typename Channel::Observer::Observing_Condition>
{
        typedef Concurrent_Observer<typename Channel::Observer::Observed_Data,
        typename Channel::Observer::Observing_Condition> Observer;

    public:
        typedef typename Channel::Buffer Buffer;
        typedef typename Channel::Address Address;

    public:
        Communicator(Channel * channel, Address address);
        
        ~Communicator();
        
        bool send(const Message * message);
        
        bool receive(Message * message);

    private:
        void update(typename Channel::Observed * obs, typename
        Channel::Observer::Observing_Condition c, Buffer * buf) {
            Observer::update(c, buf); // releases the thread waiting for data
        }
        
    private:
        Channel * _channel;
        Address _address;
};

template <typename Channel>
Communicator<Channel>::Communicator(Channel * channel, Address address): _channel(channel), _address(address) {
    _channel->attach(this, _address);
}

template <typename Channel>
Communicator<Channel>::~Communicator() {
    _channel->detach(this, _address);
}

template <typename Channel>
bool Communicator<Channel>::send(const Message * message) {
    Buffer * buf = Observer::updated(); // block until a notification is triggered
    Channel::Address from;
    int size = _channel->receive(buf, &from, message->data(), message->size());
    // . . .
    if(size > 0)
        return true;
    
    return false; // Added to ensure all paths return a value
}

template <typename Channel>
bool Communicator<Channel>::receive(Message * message) {
    Buffer * buf = Observer::updated(); // block until a notification is triggered
    Channel::Address from;
    int size = _channel->receive(buf, &from, message->data(), message->size());
    // . . .
    if(size > 0)
        return true;
    return false; // Added to ensure all paths return a value   
}

#endif // COMMUNICATOR_H
