#include "protocol.h"
#include <iostream>

// Protocol Header implementation
template <typename NIC>
class Protocol<NIC>::Header {
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

// Protocol Address implementation
template <typename NIC>
Protocol<NIC>::Address::Address() : _port(0) {
    // Default constructor
}

template <typename NIC>
Protocol<NIC>::Address::Address(const Null & null) : _port(0) {
    // Null address constructor
}

template <typename NIC>
Protocol<NIC>::Address::Address(Physical_Address paddr, Port port) : _paddr(paddr), _port(port) {
    // Full address constructor
}

// Protocol Packet implementation
template <typename NIC>
Protocol<NIC>::Packet::Packet() {
    // Initialize packet
}

template <typename NIC>
typename Protocol<NIC>::Header* Protocol<NIC>::Packet::header() {
    return this;
}

// Static member initialization
template <typename NIC>
typename Protocol<NIC>::Observed Protocol<NIC>::_observed;

// Protocol static methods implementation
template <typename NIC>
int Protocol<NIC>::send(Address from, Address to, const void* data, unsigned int size) {
    std::cout << "Protocol sending data of size " << size << std::endl;
    
    // Implementation would be:
    // Buffer * buf = NIC::alloc(to.paddr, PROTO, sizeof(Header) + size)
    // Copy data to buffer
    // Set header fields
    // NIC::send(buf)
    
    return size;
}

template <typename NIC>
int Protocol<NIC>::receive(Buffer* buf, Address from, void* data, unsigned int size) {
    std::cout << "Protocol receiving data, max size " << size << std::endl;
    
    // Implementation would be:
    // unsigned int s = NIC::receive(buf, &from.paddr, &to.paddr, data, size)
    // NIC::free(buf)
    // return s;
    
    return size;
}

template <typename NIC>
void Protocol<NIC>::attach(Observer* obs, Address address) {
    std::cout << "Protocol attaching observer" << std::endl;
    _observed.attach(obs, address._port);
}

template <typename NIC>
void Protocol<NIC>::detach(Observer* obs, Address address) {
    std::cout << "Protocol detaching observer" << std::endl;
    _observed.detach(obs, address._port);
}

// Example NIC type for instantiation
class DummyNIC {
public:
    static const unsigned int MTU = 1500;
    
    class Observer {
    public:
        Observer() {}
        virtual ~Observer() {}
    };
    
    class Observed {
    public:
        Observed() {}
        virtual ~Observed() {}
    };
    
    typedef unsigned short Protocol_Number;
    typedef int Address;
    typedef int Buffer;
};

// Explicit template instantiation for common NIC types
template class Protocol<DummyNIC>;
