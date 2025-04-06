#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <string>
#include <iostream>
#include <cstring>

#include "observed.h"
#include "nic.h"
#include "traits.h"
#include "debug.h"

// Protocol implementation that works with the real Communicator
template <typename NIC>
class Protocol: private NIC::Observer
{
    public:
        static const typename NIC::Protocol_Number PROTO = Traits<Protocol>::ETHERNET_PROTOCOL_NUMBER;
        
        typedef typename NIC::Buffer Buffer;
        typedef typename NIC::Address Physical_Address;
        typedef unsigned int Port;
        
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
        
        static const unsigned int MTU = NIC::MTU - sizeof(Header);
        typedef std::uint8_t Data[MTU];
        
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
            
            public:
                Address() = default;
                Address(const Null& null);
                Address(Physical_Address paddr, Port port);
                
                void paddr(Physical_Address addr);
                const Physical_Address& paddr() const;

                void port(Port port);
                const Port& port() const;

                static const Address BROADCAST;
                
                operator bool() const;
                bool operator==(const Address& a) const;
            
            private:
                Port _port;
                Physical_Address _paddr;
        };
        
        Protocol(NIC* nic);

        ~Protocol();
        
        int send(Address from, Address to, const void* data, unsigned int size);
        int receive(Buffer* buf, Address from, void* data, unsigned int size);

        static void attach(Observer* obs, Address address);
        static void detach(Observer* obs, Address address);
        
    private:
        void update(typename NIC::Observed * obs, typename NIC::Protocol_Number prot, Buffer * buf);

    private:
        NIC* _nic;
        static Observed _observed;
};

/******** Protocol::Address Implementation ******/
template <typename NIC>
Protocol<NIC>::Address::Address(const Null& null) : _paddr(), _port(0) {}

template <typename NIC>
Protocol<NIC>::Address::Address(Physical_Address paddr, Port port) : _paddr(paddr), _port(port) {}

template <typename NIC>
void Protocol<NIC>::Address::paddr(Physical_Address addr){
    _paddr = addr;
}

template <typename NIC>
const typename Protocol<NIC>::Physical_Address& Protocol<NIC>::Address::paddr() const{
    return _paddr;
}

template <typename NIC>
void Protocol<NIC>::Address::port(Port port) {
    _port = port;
}

template <typename NIC>
const typename Protocol<NIC>::Port& Protocol<NIC>::Address::port() const {
    return _port;
}

template <typename NIC>
Protocol<NIC>::Address::operator bool() const { 
    return _port != 0 || _paddr != Physical_Address();
}

template <typename NIC>
bool Protocol<NIC>::Address::operator==(const Address& a) const { 
    return (_paddr == a._paddr) && (_port == a._port); 
}


/********* Protocol Implementation *********/
template <typename NIC>
Protocol<NIC>::Protocol(NIC* nic) : NIC::Observer(PROTO),  _nic(nic) {
    db<Protocol>(TRC) << "Protocol<NIC>::Protocol() called!\n";
    _nic->attach(this, PROTO);

    db<Protocol>(INF) << "[Protocol] attached to NIC\n";
}

template <typename NIC>
Protocol<NIC>::~Protocol() {
    db<Protocol>(TRC) << "Protocol<NIC>::~Protocol() called!\n";
    _nic->detach(this, PROTO);

    db<Protocol>(INF) << "[Protocol] detached from NIC\n";
}

template <typename NIC>
int Protocol<NIC>::send(Address from, Address to, const void* data, unsigned int size) {
    db<Protocol>(TRC) << "Protocol<NIC>::send() called!\n";

    db<Protocol>(INF) << "Protocol sending from port " << from.port() << " to port " << to.port() << "\n";
    
    // Allocate buffer with header and data
    Buffer* buf = _nic->alloc(to.paddr(), PROTO, sizeof(Header) + size);
    if (!buf) return 0;
    
    // Set up Packet
    Packet* packet = reinterpret_cast<Packet*>(buf->data()->payload);
    packet->from_port(from.port());
    packet->to_port(to.port());
    std::memcpy(packet->template data<void>(), data, size);
    
    // Send the packet
    int result = _nic->send(buf);
    db<Protocol>(INF) << "[Protocol] NIC::send() returned value " << std::to_string(result) << "\n";
    
    if (result <= 0) {
        _nic->free(buf);
        return 0;
    }
    
    return size;
}

template <typename NIC>
int Protocol<NIC>::receive(Buffer* buf, Address from, void* data, unsigned int size) {
    db<Protocol>(TRC) << "Protocol<NIC>::receive() called!\n";

    typename NIC::Address src_mac;
    typename NIC::Address dst_mac;

    int payload_size = _nic->receive(buf, &src_mac, &dst_mac, data, size);
    db<Protocol>(INF) << "[Protocol] NIC::receive returned size " << std::to_string(payload_size) << "\n";

    if (from) {
        Packet pkt;
        std::memcpy(&pkt, buf->data(), sizeof(Packet));

        from.paddr(src_mac);              // Physical_Address
        from.port(pkt.from_port());       // Port
    }

    return payload_size;
}

template <typename NIC>
void Protocol<NIC>::attach(Observer* obs, Address address) {
    db<Protocol>(TRC) << "Protocol<NIC>::attach() called!\n";
    
    _observed.attach(obs, address.port());
    db<Protocol>(INF) << "[Protocol] Attached observer to port " << address.port() << "\n";
}

template <typename NIC>
void Protocol<NIC>::detach(Observer* obs, Address address) {
    db<Protocol>(TRC) << "Protocol<NIC>::detach() called!\n";

    _observed.detach(obs, address.port());
    db<Protocol>(INF) << "[Protocol] Detached observer from port " << address.port() << "\n";
}

template <typename NIC>
void Protocol<NIC>::update(typename NIC::Observed * obs, typename NIC::Protocol_Number prot, Buffer * buf) {
    db<Protocol>(TRC) << "Protocol<NIC>::update() called!\n";

    // If we have observers, notify them and let reference counting handle buffer cleanup
    // Otherwise, free the buffer ourselves
    if (!_observed.notify(prot, buf)) { // Use a default port for testing
        db<Protocol>(INF) << "[Protocol] data received, but no one was notified\n";
        // No observers, free the buffer
        _nic->free(buf);
    }
}

// Initialize static members
template <typename NIC>
typename Protocol<NIC>::Observed Protocol<NIC>::_observed;

// Initialize the BROADCAST address
template <typename NIC>
const typename Protocol<NIC>::Address Protocol<NIC>::Address::BROADCAST = 
    typename Protocol<NIC>::Address(
        Protocol<NIC>::Physical_Address({0xff, 0xff, 0xff, 0xff, 0xff, 0xff}), // MAC broadcast
        Protocol<NIC>::Address::NULL_VALUE // ou um valor reservado, como "porta broadcast"
    );
#endif // PROTOCOL_H