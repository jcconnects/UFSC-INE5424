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

// Stub Message class for testing
class Message {
public:
    Message(const std::string& content = "") : _content(content) {}
    
    // Copy constructor and assignment operator
    Message(const Message& other) : _content(other._content) {}
    
    Message& operator=(const Message& other) {
        if (this != &other) {
            _content = other._content;
        }
        return *this;
    }
    
    char* data() { return const_cast<char*>(_content.c_str()); }
    const char* data() const { return _content.c_str(); }
    size_t size() const { return _content.size(); }
    std::string getContent() const { return _content; }
    
private:
    std::string _content;
};

// Buffer stub with reference counting
struct BufferStub {
    std::string data;
    std::atomic<int> ref_count;
    
    BufferStub(const std::string& content = "") : data(content), ref_count(0) {}
};

// Stub for Channel/Protocol
class ProtocolStub {
public:
    typedef BufferStub Buffer;
    typedef std::string Physical_Address;
    typedef int Port;
    
    class Address {
    public:
        enum Null { NULL_VALUE };
        
        typedef int Port; // Add nested Port type for compatibility
        
        Port _port;
        Physical_Address _paddr;
        
        Address() : _port(0), _paddr("") {}
        Address(Physical_Address paddr, Port port) : _port(port), _paddr(paddr) {}
        
        static const Address BROADCAST;
        
        operator bool() const { return !_paddr.empty() || _port != 0; }
        bool operator==(const Address& a) const { 
            return (_paddr == a._paddr) && (_port == a._port); 
        }
    };
    
    ProtocolStub() {
        std::cout << "Created Protocol Stub" << std::endl;
    }
    
    ~ProtocolStub() {
        std::cout << "Destroyed Protocol Stub" << std::endl;
    }
    
    // This is what the Communicator will call to send a message
    int send(Address from, Address to, const void* data, unsigned int size) {
        std::lock_guard<std::mutex> lock(_mutex);
        
        std::string message(static_cast<const char*>(data), size);
        std::cout << "Protocol sending message from port " << from._port 
                  << " to " << (to == Address::BROADCAST ? "BROADCAST" : std::to_string(to._port))
                  << ": " << message << std::endl;
        
        // Store the message for later verification
        _sent_messages.push_back(message);
        
        return size;
    }
    
    // This is what the Communicator will call to receive a message
    int receive(Buffer* buf, Address* from, void* data, unsigned int size) {
        std::lock_guard<std::mutex> lock(_mutex);
        
        if (!buf) return 0;
        
        // Copy the buffer data to the output buffer
        size_t copy_size = std::min<size_t>(size, buf->data.size());
        memcpy(data, buf->data.c_str(), copy_size);
        
        // Set the sender address if provided
        if (from) {
            *from = Address("sender_address", 999);
        }
        
        std::cout << "Protocol received message: " << buf->data << std::endl;
        
        return copy_size;
    }
    
    // Simulate async notification
    void simulateReceive(const std::string& message, Port port) {
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
    
    // Observer management
    void attach(Concurrent_Observer<Buffer, Port>* obs, Address address) {
        _observed.attach(obs, address._port);
        std::cout << "Observer attached to port " << address._port << std::endl;
    }
    
    void detach(Concurrent_Observer<Buffer, Port>* obs, Address address) {
        _observed.detach(obs, address._port);
        std::cout << "Observer detached from port " << address._port << std::endl;
    }
    
    // Test helpers
    bool hasMessage(const std::string& message) const {
        std::lock_guard<std::mutex> lock(_mutex);
        for (const auto& msg : _sent_messages) {
            if (msg == message) {
                return true;
            }
        }
        return false;
    }
    
    size_t getSentCount() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _sent_messages.size();
    }
    
    void clearSentMessages() {
        std::lock_guard<std::mutex> lock(_mutex);
        _sent_messages.clear();
    }
    
private:
    mutable std::mutex _mutex;
    std::vector<std::string> _sent_messages;
    Concurrent_Observed<Buffer, Port> _observed;
};

// Initialize static BROADCAST address
const ProtocolStub::Address ProtocolStub::Address::BROADCAST("255.255.255.255", 0);

// Stub for NIC
class NICStub {
public:
    typedef std::string Address;
    typedef int Protocol_Number;
    
    NICStub() {
        std::cout << "Created NIC Stub" << std::endl;
    }
    
    ~NICStub() {
        std::cout << "Destroyed NIC Stub" << std::endl;
    }
    
    Address address() const { return "aa:bb:cc:dd:ee:ff"; }
};

#endif // COMMUNICATOR_STUBS_H 