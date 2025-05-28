#ifndef COMMUNICATOR_H
#define COMMUNICATOR_H

#include "../traits.h"
#include "message.h"
#include "../util/observer.h"
#include "../util/debug.h"

template <typename Channel>
class Communicator: public Concurrent_Observer<typename Channel::Observer::Observed_Data, typename Channel::Observer::Observing_Condition>
{
    
    public:
        typedef Concurrent_Observer<typename Channel::Observer::Observed_Data, typename Channel::Observer::Observing_Condition> Observer;
        typedef typename Channel::Buffer Buffer;
        typedef typename Channel::Address Address;
        typedef typename Channel::Port Port;
        typedef Message<Channel> Message_T;
        static constexpr const unsigned int MAX_MESSAGE_SIZE = Channel::MTU; // Maximum message size in bytes

        // Constructor and Destructor
        Communicator(Channel* channel, Address address);
        ~Communicator();
        
        // Communication methods
        bool send(const Message_T* message);
        bool receive(Message_T* message);


        // Address getter
        const Address& address() const;

        // Release thread waiting for buffer
        void release();

        // Atomic variable running
        std::atomic<bool> _running;

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
};

/*************** Communicator Implementation *****************/
template <typename Channel>
Communicator<Channel>::Communicator(Channel* channel, Address address) : Observer(address.port()), _channel(channel), _address(address) {
    _channel->attach(this, address);
    _running.store(true, std::memory_order_release);
}

template <typename Channel>
Communicator<Channel>::~Communicator() {
    db<Communicator>(TRC) << "Communicator<Channel>::~Communicator() called for address: " << _address.to_string() << "\n";
    
    _channel->detach(this, _address);
    db<Communicator>(INF) << "[Communicator] Channel detached from address: " << _address.to_string() << "\n";
}

template <typename Channel>
bool Communicator<Channel>::send(const Message_T* message) {
    db<Communicator>(TRC) << "Communicator<Channel>::send() called!\n";
    
    if (!_running.load(std::memory_order_acquire)) {
        db<Communicator>(WRN) << "[Communicator] Not running, skipping send!\n";
        return false;
    }

    int result = _channel->send(_address, Address::BROADCAST, message->data(), message->size());
    db<Communicator>(INF) << "[Communicator] Channel::send() return value " << std::to_string(result) << "\n";
    
    return result;
}

template <typename Channel>
bool Communicator<Channel>::receive(Message_T* message) {
    db<Communicator>(TRC) << "Communicator<Channel>::receive() called!\n";
    
    if (!_running.load(std::memory_order_acquire)) {
        db<Communicator>(WRN) << "[Communicator] Not running, skipping receive!\n";
        return false;
    }

    Buffer* buf = Observer::updated();
    if (!buf) {
        db<Communicator>(WRN) << "[Communicator] No buffer available for receiving message!\n";
        return false;
    }

    std::uint8_t temp_data[MAX_MESSAGE_SIZE];

    int result = _channel->receive(buf, nullptr, temp_data, buf->size()); // Assuming Channel::receive fills 'from'
    db<Communicator>(INF) << "[Communicator] Channel::receive() returned " << result << "\n";

    if (result <= 0)
        return false;

    // Deserialize the raw data into the message
    *message = Message_T::deserialize(temp_data, result);
    db<Communicator>(INF) << "[Communicator] Received message from: " << message->origin().to_string() << "\n";

    return true;
}

template <typename Channel>
void Communicator<Channel>::release() {
    _running.store(false, std::memory_order_release);
    update(nullptr, this->rank(), nullptr);
}

template <typename Channel>
void Communicator<Channel>::update(typename Channel::Observed* obs, typename Channel::Observer::Observing_Condition c, Buffer* buf) {
    Observer::update(c, buf); // releases the thread waiting for data
}

template <typename Channel>
const typename Communicator<Channel>::Address& Communicator<Channel>::address() const {
    return _address;
}

#endif // COMMUNICATOR_H
