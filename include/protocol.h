#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "nic.h"
#include "observed.h"
#include <string>
#include <iostream>
#include <cstring>

// Protocol implementation that works with the real Communicator
template <typename NICType>
class Protocol: private NICType::Observer
{
public:
    static const typename NICType::Protocol_Number PROTO = Traits<Protocol>::ETHERNET_PROTOCOL_NUMBER;
    
    typedef typename NICType::Buffer Buffer;
    typedef typename NICType::Address Physical_Address;
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
        
    private:
        Data _data;
    } __attribute__((packed));
    
    // Address class for Protocol layer
    class Address
    {
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
    
    static const unsigned int MTU = NICType::MTU - sizeof(Header);
    typedef unsigned char Data[MTU];
    
    Protocol(NICType* nic);
    ~Protocol();
    
    static int send(Address from, Address to, const void* data, unsigned int size);
    static int receive(Buffer* buf, Address* from, void* data, unsigned int size);
    static void attach(Observer* obs, Address address);
    static void detach(Observer* obs, Address address);
    void update(typename NICType::Protocol_Number prot, Buffer* buf) override;
    
private:
    NICType* _nic;
    static Observed _observed;
};

// Address class implementations
template <typename NICType>
Protocol<NICType>::Address::Address() : _port(0), _paddr("") {}

template <typename NICType>
Protocol<NICType>::Address::Address(const Null& null) : _port(0), _paddr("") {}

template <typename NICType>
Protocol<NICType>::Address::Address(Physical_Address paddr, Port port) : _port(port), _paddr(paddr) {}

template <typename NICType>
Protocol<NICType>::Address::operator bool() const { 
    return !_paddr.empty() || _port != 0; 
}

template <typename NICType>
bool Protocol<NICType>::Address::operator==(const Address& a) const { 
    return (_paddr == a._paddr) && (_port == a._port); 
}

// Protocol class implementations
template <typename NICType>
Protocol<NICType>::Protocol(NICType* nic) 
    : _nic(nic) {
    std::cout << "Protocol created" << std::endl;
    _nic->attach(this, PROTO);
}

template <typename NICType>
Protocol<NICType>::~Protocol() {
    std::cout << "Protocol destroyed" << std::endl;
    _nic->detach(this, PROTO);
}

template <typename NICType>
int Protocol<NICType>::send(Address from, Address to, const void* data, unsigned int size) {
    std::cout << "Protocol sending from port " << from._port << " to port " << to._port << std::endl;
    
    // Allocate buffer with header and data
    Buffer* buf = _nic->alloc(to._paddr, PROTO, sizeof(Header) + size);
    if (!buf) return 0;
    
    // Set up packet header
    Packet* packet = reinterpret_cast<Packet*>(buf);
    packet->from_port(from._port);
    packet->to_port(to._port);
    packet->size(size);
    
    // Copy data
    memcpy(packet->data<void>(), data, size);
    
    // Send the packet
    int result = _nic->send(buf);
    if (result <= 0) {
        _nic->free(buf);
        return 0;
    }
    
    return size;
}

template <typename NICType>
int Protocol<NICType>::receive(Buffer* buf, Address* from, void* data, unsigned int size) {
    std::cout << "Protocol receiving buffer" << std::endl;
    
    if (!buf) return 0;
    
    // Get source address from NIC
    Physical_Address src_addr;
    Physical_Address dst_addr;
    
    // Process the packet
    Packet* packet = reinterpret_cast<Packet*>(buf);
    int received = _nic->receive(buf, &src_addr, &dst_addr, data, size);
    
    // Set sender address
    if (from) {
        from->_paddr = src_addr;
        from->_port = packet->from_port();
    }
    
    return received;
}

template <typename NICType>
void Protocol<NICType>::attach(Observer* obs, Address address) {
    std::cout << "Protocol: Attaching observer to port " << address._port << std::endl;
    _observed.attach(obs, address._port);
}

template <typename NICType>
void Protocol<NICType>::detach(Observer* obs, Address address) {
    std::cout << "Protocol: Detaching observer from port " << address._port << std::endl;
    _observed.detach(obs, address._port);
}

template <typename NICType>
void Protocol<NICType>::update(typename NICType::Protocol_Number prot, Buffer* buf) {
    std::cout << "Protocol: Received notification from NIC for protocol " << prot << std::endl;
    
    // If we have observers, notify them and let reference counting handle buffer cleanup
    // Otherwise, free the buffer ourselves
    if (!_observed.notify(999, buf)) { // Use a default port for testing
        // No observers, free the buffer
        _nic->free(buf);
    }
}

// Initialize static members
template <typename NICType>
typename Protocol<NICType>::Observed Protocol<NICType>::_observed;

// Initialize the BROADCAST address
template <typename NICType>
const typename Protocol<NICType>::Address Protocol<NICType>::Address::BROADCAST("255.255.255.255", 0);

#endif // PROTOCOL_H