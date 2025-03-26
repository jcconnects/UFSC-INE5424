#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "nic.h"
#include "observed.h"
#include <string>
#include <iostream>
#include <cstring>

// Protocol implementation that works with the real Communicator
template <typename NICType>
class Protocol: private typename NICType::Observer
{
public:
    static const typename NICType::Protocol_Number PROTO = 0; // Default protocol number
    
    typedef typename NICType::Buffer Buffer;
    typedef typename NICType::Address Physical_Address;
    typedef int Port;
    
    typedef Conditional_Data_Observer<Buffer, Port> Observer;
    typedef Conditionally_Data_Observed<Buffer, Port> Observed;
    
    // Address class for Protocol layer
    class Address
    {
    public:
        enum Null { NULL_VALUE };
        
        // Fix the Port typedef conflict
        typedef int PortType; // Different name to avoid conflict
        
        Port _port;
        Physical_Address _paddr;
        
        Address() : _port(0), _paddr("") {}
        Address(const Null& null) : _port(0), _paddr("") {}
        Address(Physical_Address paddr, Port port) : _port(port), _paddr(paddr) {}
        
        static const Address BROADCAST;
        
        operator bool() const { return !_paddr.empty() || _port != 0; }
        bool operator==(const Address& a) const { 
            return (_paddr == a._paddr) && (_port == a._port); 
        }
    };
    
    // Constructor - properly initialize the Conditional_Data_Observer
    Protocol(NICType* nic) 
        : _nic(nic), _observed() {
        std::cout << "Protocol created" << std::endl;
        this->_rank = PROTO; // Set the rank/protocol number directly
        _nic->attach(this, PROTO);
    }
    
    // Destructor
    ~Protocol() {
        std::cout << "Protocol destroyed" << std::endl;
        _nic->detach(this, PROTO);
    }
    
    // Send data to specified destination
    int send(Address from, Address to, const void* data, unsigned int size) {
        std::cout << "Protocol sending from port " << from._port << " to port " << to._port << std::endl;
        
        // Simply pass to NIC layer for now
        return _nic->send(to._paddr, PROTO, data, size);
    }
    
    // Receive data from a buffer
    int receive(Buffer* buf, Address* from, void* data, unsigned int size) {
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
    
    // Observer registration
    void attach(Observer* obs, Address address) {
        std::cout << "Protocol: Attaching observer to port " << address._port << std::endl;
        _observed.attach(obs, address._port);
    }
    
    // Observer deregistration
    void detach(Observer* obs, Address address) {
        std::cout << "Protocol: Detaching observer from port " << address._port << std::endl;
        _observed.detach(obs, address._port);
    }
    
    // Process a notification from the NIC layer - implementing the virtual method from Conditional_Data_Observer
    void update(typename NICType::Protocol_Number prot, Buffer* buf) override {
        std::cout << "Protocol: Received notification from NIC for protocol " << prot << std::endl;
        
        // Create a copy of the buffer for each observer
        if (!_observed.notify(999, buf)) { // Use a default port for testing
            // No observers, free the buffer
            _nic->free(buf);
        }
    }
    
private:
    NICType* _nic;
    Observed _observed;
};

// Initialize the BROADCAST address
template <typename NICType>
const typename Protocol<NICType>::Address Protocol<NICType>::Address::BROADCAST("255.255.255.255", 0);

#endif // PROTOCOL_H
