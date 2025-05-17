#ifndef COMMUNICATOR_H
#define COMMUNICATOR_H

#include <stdexcept>

#include "observed.h"
#include "observer.h"
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
        typedef Conditionally_Data_Observed<Message, Message::Unit> Observed;
        typedef Concurrent_Observer<Message, Message::Unit> Agent;

        static constexpr const unsigned int MAX_MESSAGE_SIZE = Channel::MTU; // Maximum message size in bytes

        // Constructor and Destructor
        Communicator(Channel* channel, Address address);
        ~Communicator();
        
        // Communication methods
        bool send(const Message& message, const bool is_internal = false);
        bool receive(Message* message);

        // Address getter
        const Address& address() const;
        
        void attach(Agent* agent, Message::Unit unit);
        void detach(Agent* agent, Message::Unit unit);

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
        Observed _observed;
};

/*************** Communicator Implementation *****************/
template <typename Channel>
Communicator<Channel>::Communicator(Channel* channel, Address address) : Observer(address.port()), _channel(channel), _address(address), _closed(false) {
    _channel->attach(this, address);
}

template <typename Channel>
Communicator<Channel>::~Communicator() {
    _channel->detach(this, _address);
}

template <typename Channel>
bool Communicator<Channel>::send(const Message& message, const bool is_internal) {
    db<Communicator>(TRC) << "Communicator<Channel>::send() called!\n";

    if (message.size() > MAX_MESSAGE_SIZE) {
        db<Communicator>(ERR) << "[Communicator] message too big!\n";
        return false; 
    }
    
    int result;
    if (is_internal) {
        result = _channel->send(_address, Address::INTERNAL_BROADCAST, message.data(), message.size());
    } else {
        result = _channel->send(_address, Address::EXTERNAL_BROADCAST, message.data(), message.size());
    }
    db<Communicator>(INF) << "[Communicator] Channel::send() return value " << std::to_string(result) << "\n";
        
    if (result <= 0)
        return false;

    return true;
}

template <typename Channel>
bool Communicator<Channel>::receive(Message* message) {
    db<Communicator>(TRC) << "Communicator<Channel>::receive() called!\n";
    
    Buffer* buf = Observer::updated();
    db<Communicator>(INF) << "[Communicator] buffer retrieved\n";

    if (!buf) {
        return false;
    }

    std::uint8_t temp_data[MAX_MESSAGE_SIZE];

    int size = _channel->receive(buf, nullptr, temp_data, buf->size()); // Assuming Channel::receive fills 'from'

    if (size <= 0) {
        db<Communicator>(ERR) << "[Communicator] failed to receive data.\n";
        return false;
    }

    // Deserialize the raw data into the message
    *message = Message::deserialize(temp_data, size);
    db<Communicator>(INF) << "[Communicator] Received message from: " << message->origin().to_string() << "\n";

    return true;
}

template <typename Channel>
void Communicator<Channel>::attach(Agent* agent, Message::Unit type) {
    _observed.attach(agent, type);
}

template <typename Channel>
void Communicator<Channel>::detach(Agent* agent, Message::Unit type) {
    _observed.detach(agent, type);
}

template <typename Channel>
void Communicator<Channel>::update(typename Channel::Observed* obs, typename Channel::Observer::Observing_Condition c, Buffer* buf) {
    Observer::update(c, buf); // releases the thread waiting for data
    
    Message msg;
    if (receive(&msg)){
        switch (msg.message_type()){
            case Message::Type::INTEREST:
                _observed.notify(msg.unit_type(), msg);
                break;
            case Message::Type::RESPONSE:
                _observed.notifyAll();
        }
    }
}

template <typename Channel>
const typename Communicator<Channel>::Address& Communicator<Channel>::address() const {
    return _address;
}

#endif // COMMUNICATOR_H
