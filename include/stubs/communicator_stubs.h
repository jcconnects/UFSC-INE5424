#ifndef COMMUNICATOR_STUBS_H
#define COMMUNICATOR_STUBS_H

#include <iostream>
#include <string>
#include <atomic>
#include <mutex>
#include <vector>
#include <memory>
#include <thread>
#include <cstring>

// Forward declarations - we need full definitions
#include "../observed.h"
#include "../observer.h"

// Implementations of 
// - Buffer
// - Message
// - ProtocolStub
// - NICStub

// Stub Message class for testing
class Message {
public:
    Message(const std::string& content = "");
    
    // Copy constructor and assignment operator
    Message(const Message& other);
    Message& operator=(const Message& other);
    
    char* data();
    const char* data() const;
    size_t size() const;
    std::string getContent() const;
    
private:
    std::string _content;
};

// Message implementations
Message::Message(const std::string& content) : _content(content) {}

Message::Message(const Message& other) : _content(other._content) {}

Message& Message::operator=(const Message& other) {
    if (this != &other) {
        _content = other._content;
    }
    return *this;
}

char* Message::data() { 
    return const_cast<char*>(_content.c_str()); 
}

const char* Message::data() const { 
    return _content.c_str(); 
}

size_t Message::size() const { 
    return _content.size(); 
}

std::string Message::getContent() const { 
    return _content; 
}

// Buffer stub with reference counting
struct BufferStub {
    std::string data;
    std::atomic<int> ref_count;
    
    BufferStub(const std::string& content = "");
};

// BufferStub implementation
BufferStub::BufferStub(const std::string& content) : data(content), ref_count(0) {}

// Stub for Channel/Protocol
class ProtocolStub {
public:
    typedef BufferStub Buffer;
    typedef std::string Physical_Address;
    typedef int Port;
    
    typedef Conditional_Data_Observer<Buffer, Port> Observer;
    typedef Conditionally_Data_Observed<Buffer, Port> Observed;
    
    // Header class for protocol messages
    class Header {
    public:
        Header() : _from_port(0), _to_port(0), _size(0) {}
        
        Port from_port() const { return _from_port; }
        void from_port(Port p) { _from_port = p; }
        
        Port to_port() const { return _to_port; }
        void to_port(Port p) { _to_port = p; }
        
        unsigned int size() const { return _size; }
        void size(unsigned int s) { _size = s; }
        
    private:
        Port _from_port;
        Port _to_port;
        unsigned int _size;
    };
    
    // Packet class that includes header and data
    class Packet: public Header {
    public:
        Packet() {}
        
        Header* header() { return this; }
        
        template<typename T>
        T* data() { return reinterpret_cast<T*>(&_data); }
        
        template<typename T>
        const T* data() const { return reinterpret_cast<const T*>(&_data); }
        
    private:
        unsigned char _data[1500]; // MTU size
    } __attribute__((packed));
    
    class Address {
    public:
        enum Null { NULL_VALUE };
        
        Port _port;
        Physical_Address _paddr;
        
        Address();
        Address(const Null& null);
        Address(Physical_Address paddr, Port port);
        
        static const Address BROADCAST;
        
        operator bool() const;
        bool operator==(const Address& a) const;
    };
    
    static const unsigned int MTU = 1500 - sizeof(Header);
    typedef unsigned char Data[MTU];
    
    ProtocolStub();
    ~ProtocolStub();
    
    int send(Address from, Address to, const void* data, unsigned int size);
    int receive(Buffer* buf, Address* from, void* data, unsigned int size);
    void simulateReceive(const std::string& message, Port port);
    
    // Observer management
    void attach(Observer* obs, Address address);
    void detach(Observer* obs, Address address);
    
    // Test helpers
    bool hasMessage(const std::string& message) const;
    size_t getSentCount() const;
    void clearSentMessages();
    
private:
    mutable std::mutex _mutex;
    std::vector<std::string> _sent_messages;
    Observed _observed;
};

// ProtocolStub::Address implementations
ProtocolStub::Address::Address() : _port(0), _paddr("") {}

ProtocolStub::Address::Address(const Null& null) : _port(0), _paddr("") {}

ProtocolStub::Address::Address(Physical_Address paddr, Port port) : _port(port), _paddr(paddr) {}

ProtocolStub::Address::operator bool() const { 
    return !_paddr.empty() || _port != 0; 
}

bool ProtocolStub::Address::operator==(const Address& a) const { 
    return (_paddr == a._paddr) && (_port == a._port); 
}

// ProtocolStub implementations
ProtocolStub::ProtocolStub() {
    std::cout << "Created Protocol Stub" << std::endl;
}

ProtocolStub::~ProtocolStub() {
    std::cout << "Destroyed Protocol Stub" << std::endl;
}

int ProtocolStub::send(Address from, Address to, const void* data, unsigned int size) {
    std::lock_guard<std::mutex> lock(_mutex);
    
    // Create a packet with header and data
    Packet packet;
    packet.from_port(from._port);
    packet.to_port(to._port);
    packet.size(size);
    
    // Copy data into packet
    memcpy(packet.data<void>(), data, size);
    
    // Store the message for later verification
    std::string message(static_cast<const char*>(data), size);
    _sent_messages.push_back(message);
    
    std::cout << "Protocol sending message from port " << from._port 
              << " to " << (to == Address::BROADCAST ? "BROADCAST" : std::to_string(to._port))
              << ": " << message << std::endl;
    
    return size;
}

int ProtocolStub::receive(Buffer* buf, Address* from, void* data, unsigned int size) {
    std::lock_guard<std::mutex> lock(_mutex);
    
    if (!buf) return 0;
    
    // Process the packet
    const Packet* packet = reinterpret_cast<const Packet*>(buf->data.c_str());
    
    // Copy data from packet
    size_t copy_size = std::min<size_t>(size, packet->size());
    memcpy(data, packet->data<void>(), copy_size);
    
    // Set the sender address if provided
    if (from) {
        from->_paddr = "sender_address";
        from->_port = packet->from_port();
    }
    
    std::cout << "Protocol received message: " << std::string(static_cast<char*>(data), copy_size) << std::endl;
    
    return copy_size;
}

void ProtocolStub::simulateReceive(const std::string& message, Port port) {
    std::lock_guard<std::mutex> lock(_mutex);
    
    // Create a new buffer with the message
    auto buf = new Buffer(message);
    buf->ref_count = 0; // Reset reference count
    
    // Notify observers for this port
    bool notified = _observed.notify(port, buf);
    
    if (!notified) {
        std::cout << "No observers for port " << port << ", deleting buffer" << std::endl;
        delete buf;
    } else {
        std::cout << "Notified observers for port " << port << " with message: " << message << std::endl;
    }
}

void ProtocolStub::attach(Observer* obs, Address address) {
    _observed.attach(obs, address._port);
    std::cout << "Observer attached to port " << address._port << std::endl;
}

void ProtocolStub::detach(Observer* obs, Address address) {
    _observed.detach(obs, address._port);
    std::cout << "Observer detached from port " << address._port << std::endl;
}

bool ProtocolStub::hasMessage(const std::string& message) const {
    std::lock_guard<std::mutex> lock(_mutex);
    for (const auto& msg : _sent_messages) {
        if (msg == message) {
            return true;
        }
    }
    return false;
}

size_t ProtocolStub::getSentCount() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _sent_messages.size();
}

void ProtocolStub::clearSentMessages() {
    std::lock_guard<std::mutex> lock(_mutex);
    _sent_messages.clear();
}

// Initialize static BROADCAST address
const ProtocolStub::Address ProtocolStub::Address::BROADCAST("255.255.255.255", 0);

// Stub for NIC
class NICStub {
public:
    typedef std::string Address;
    typedef int Protocol_Number;
    
    NICStub();
    ~NICStub();
    
    Address address() const;
};

// NICStub implementations
NICStub::NICStub() {
    std::cout << "Created NIC Stub" << std::endl;
}

NICStub::~NICStub() {
    std::cout << "Destroyed NIC Stub" << std::endl;
}

NICStub::Address NICStub::address() const { 
    return "aa:bb:cc:dd:ee:ff"; 
}

#endif // COMMUNICATOR_STUBS_H 