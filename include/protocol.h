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
    static const typename NICType::Protocol_Number PROTO = 0; // Default protocol number
    
    typedef typename NICType::Buffer Buffer;
    typedef typename NICType::Address Physical_Address;
    typedef int Port;
    
    typedef Concurrent_Observer<Buffer, Port> Observer;
    typedef Concurrent_Observed<Buffer, Port> Observed;
    
    // Address class for Protocol layer
    class Address
    {
    public:
        enum Null { NULL_VALUE };
        
        // Fix the Port typedef conflict
        typedef int PortType; // Different name to avoid conflict
        
        Port _port;
        Physical_Address _paddr;
        
        Address();
        Address(const Null& null);
        Address(Physical_Address paddr, Port port);
        
        static const Address BROADCAST;
        
        operator bool() const;
        bool operator==(const Address& a) const;
    };
    
    Protocol(NICType* nic);
    ~Protocol();
    int send(Address from, Address to, const void* data, unsigned int size);
    int receive(Buffer* buf, Address* from, void* data, unsigned int size);
    void attach(Observer* obs, Address address);
    void detach(Observer* obs, Address address);
    void update(typename NICType::Protocol_Number prot, Buffer* buf) override;
    
private:
    NICType* _nic;
    Observed _observed;
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
    : _nic(nic), _observed() {
    std::cout << "Protocol created" << std::endl;
    this->_rank = PROTO; // Set the rank/protocol number directly
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
    
    // Simply pass to NIC layer for now
    return _nic->send(to._paddr, PROTO, data, size);
}

template <typename NICType>
int Protocol<NICType>::receive(Buffer* buf, Address* from, void* data, unsigned int size) {
    std::cout << "Protocol receiving buffer" << std::endl;
    
    if (!buf) return 0;
    
    // Get source address from NIC
    Physical_Address src_addr;
    Physical_Address dst_addr;
    
    int received = _nic->receive(buf, &src_addr, &dst_addr, data, size);
    
    // Set sender address
    if (from) {
        from->_paddr = src_addr;
        from->_port = 999; // Simulate sender port
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

// Initialize the BROADCAST address
template <typename NICType>
const typename Protocol<NICType>::Address Protocol<NICType>::Address::BROADCAST("255.255.255.255", 0);

#endif // PROTOCOL_H