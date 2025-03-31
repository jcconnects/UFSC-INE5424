// Initializer.h
#ifndef STUB_H
#define STUB_H

#include <memory>
#include <string>
#include <unistd.h>
#include <iostream>

// Forward declarations of classes defined in other files
class Vehicle;
class Initializer;

// Stub Message class (instead of including "message.h")
class Message {
public:
    Message(const std::string& content = "");
    const std::string& data() const;
    size_t size() const;
    
private:
    std::string _content;
};

// Message implementations
Message::Message(const std::string& content) : _content(content) {}

const std::string& Message::data() const { 
    return _content; 
}

size_t Message::size() const { 
    return _content.size(); 
}

// Stub SocketEngine class (instead of including "engine.h")
class SocketEngine {
public:
    SocketEngine();
    ~SocketEngine();
    std::string address() const;
};

// SocketEngine implementations
SocketEngine::SocketEngine() {
    std::cout << "[Engine] Created SocketEngine" << std::endl;
}

SocketEngine::~SocketEngine() {
    std::cout << "[Engine] Destroyed SocketEngine" << std::endl;
}

std::string SocketEngine::address() const { 
    return "00:11:22:33:44:55"; 
}

// Stub NIC class
template <typename Engine>
class NIC {
public:
    typedef std::string Address;
    typedef int Protocol_Number;
    
    NIC();
    ~NIC();
    
    std::string address() const;
    void attach(void* obs, Protocol_Number prot);
    void detach(void* obs, Protocol_Number prot);
};

// NIC implementations
template <typename Engine>
NIC<Engine>::NIC() {
    std::cout << "[NIC] Created NIC" << std::endl;
}

template <typename Engine>
NIC<Engine>::~NIC() {
    std::cout << "[NIC] Destroyed NIC" << std::endl;
}

template <typename Engine>
std::string NIC<Engine>::address() const { 
    return "aa:bb:cc:dd:ee:ff"; 
}

template <typename Engine>
void NIC<Engine>::attach(void* obs, Protocol_Number prot) {
    std::cout << "[NIC] Observer attached" << std::endl;
}

template <typename Engine>
void NIC<Engine>::detach(void* obs, Protocol_Number prot) {
    std::cout << "[NIC] Observer detached" << std::endl;
}

// Stub Protocol class
template <typename N>
class Protocol {
public:
    static const typename N::Protocol_Number PROTO = 0x800;
    typedef typename N::Address Physical_Address;
    typedef int Port;
    
    class Address {
    public:
        Address();
        Address(Physical_Address paddr, Port port);
        static const Address BROADCAST;
    };
    
    Protocol(N* nic);
    ~Protocol();
    
    bool send(const void* data, unsigned int size);
    bool receive(void* data, unsigned int size);
    void attach(void* obs, Address address);
    void detach(void* obs, Address address);
    
private:
    N* _nic;
};

// Protocol implementations
template <typename N>
Protocol<N>::Protocol(N* nic) : _nic(nic) {
    std::cout << "[Protocol] Created Protocol" << std::endl;
    _nic->attach(this, PROTO);
}

template <typename N>
Protocol<N>::~Protocol() {
    std::cout << "[Protocol] Destroyed Protocol" << std::endl;
    _nic->detach(this, PROTO);
}

template <typename N>
bool Protocol<N>::send(const void* data, unsigned int size) {
    std::cout << "[Protocol] Sending message" << std::endl;
    return true;
}

template <typename N>
bool Protocol<N>::receive(void* data, unsigned int size) {
    std::cout << "[Protocol] Receiving message" << std::endl;
    return true;
}

template <typename N>
void Protocol<N>::attach(void* obs, Address address) {
    std::cout << "[Protocol] Observer attached" << std::endl;
}

template <typename N>
void Protocol<N>::detach(void* obs, Address address) {
    std::cout << "[Protocol] Observer detached" << std::endl;
}

template <typename N>
Protocol<N>::Address::Address() {}

template <typename N>
Protocol<N>::Address::Address(Physical_Address paddr, Port port) {}

template <typename N>
const typename Protocol<N>::Address Protocol<N>::Address::BROADCAST;

// Stub Communicator class
template <typename C>
class Communicator {
public:
    typedef typename C::Address Address;
    
    Communicator(C* channel, Address address);
    ~Communicator();
    
    bool send(const Message* message);
    bool receive(Message* message);
    
private:
    C* _channel;
    Address _address;
};

// Communicator implementations
template <typename C>
Communicator<C>::Communicator(C* channel, Address address) : _channel(channel), _address(address) {
    std::cout << "[Communicator] Created Communicator" << std::endl;
    _channel->attach(this, address);
}

template <typename C>
Communicator<C>::~Communicator() {
    std::cout << "[Communicator] Destroyed Communicator" << std::endl;
    _channel->detach(this, _address);
}

template <typename C>
bool Communicator<C>::send(const Message* message) {
    std::cout << "[Communicator] Sending message: " << message->data() << std::endl;
    return true;
}

template <typename C>
bool Communicator<C>::receive(Message* message) {
    std::cout << "[Communicator] Receiving message" << std::endl;
    return true;
}

// Forward declarations for templated classes
template <typename E> class NIC;
template <typename N> class Protocol;
template <typename C> class Communicator;

#endif // STUB_H