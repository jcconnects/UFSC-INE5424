#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <string>
#include <iostream>
#include <cstring>
#include <atomic>

#include "nic.h"
#include "traits.h"
#include "debug.h"
#include "observed.h"

// Protocol implementation that works with the real Communicator
template <typename NIC>
class Protocol: private NIC::Observer
{
    public:
        static const typename NIC::Protocol_Number PROTO = Traits<Protocol>::ETHERNET_PROTOCOL_NUMBER;
        
        typedef typename NIC::DataBuffer Buffer;
        typedef typename NIC::Address Physical_Address;
        typedef unsigned int Port;
        
        // Change to use Concurrent_Observer for Communicator interactions
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
                Address();
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
        
        // Check if protocol can still process messages
        bool active() const { return _nic && _active.load(std::memory_order_acquire); }
        
        // Signal protocol to stop processing
        void signal_stop() { 
            db<Protocol>(TRC) << "Protocol::signal_stop() called!\n";
            _active.store(false, std::memory_order_release); 
        }
        
        // Reactivate protocol for processing
        void reactivate() {
            db<Protocol>(TRC) << "Protocol::reactivate() called!\n";
            _active.store(true, std::memory_order_release);
        }
        
    private:
        void update(typename NIC::Protocol_Number prot, Buffer * buf) override;

    private:
        NIC* _nic;
        std::atomic<bool> _active;
        static Observed _observed;
};

/******** Protocol::Address Implementation ******/
template <typename NIC>
Protocol<NIC>::Address::Address() : _port(0), _paddr() {}

template <typename NIC>
Protocol<NIC>::Address::Address(const Null& null) : _port(0), _paddr() {}

template <typename NIC>
Protocol<NIC>::Address::Address(Physical_Address paddr, Port port) : _port(port), _paddr(paddr) {}

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
Protocol<NIC>::Protocol(NIC* nic) : NIC::Observer(PROTO), _nic(nic), _active(true) {
    db<Protocol>(TRC) << "Protocol<NIC>::Protocol() called!\n";
    _nic->attach(this, PROTO);

    db<Protocol>(INF) << "[Protocol] attached to NIC\n";
}

template <typename NIC>
Protocol<NIC>::~Protocol() {
    db<Protocol>(TRC) << "Protocol<NIC>::~Protocol() called!\n";
    
    // Set active to false to prevent any further processing
    signal_stop();
    
    if (_nic) {
        _nic->detach(this, PROTO);
        db<Protocol>(INF) << "[Protocol] detached from NIC\n";
    }
}

template <typename NIC>
int Protocol<NIC>::send(Address from, Address to, const void* data, unsigned int size) {
    db<Protocol>(TRC) << "Protocol<NIC>::send() called!\n";
    
    // Check if protocol is still active before sending
    if (!active()) {
        db<Protocol>(WRN) << "[Protocol] send() called while protocol is inactive\n";
        return 0;
    }

    db<Protocol>(INF) << "[Protocol] sending from port " << from.port() << " to port " << to.port() << "\n";
    
    // Allocate buffer with header and data
    unsigned int frame_size = sizeof(Header) + Ethernet::HEADER_SIZE + size;

    Buffer* buf = _nic->alloc(to.paddr(), PROTO, frame_size);
    if (!buf) {
        db<Protocol>(ERR) << "[Protocol] Failed to allocate buffer for send\n";
        return 0;
    }
    
    // Check again if protocol is still active
    if (!active()) {
        db<Protocol>(WRN) << "[Protocol] Protocol became inactive during send, freeing buffer\n";
        _nic->free(buf);
        return 0;
    }
    
    // Set up Packet
    Packet* packet = reinterpret_cast<Packet*>(buf->data()->payload);
    packet->from_port(from.port());
    packet->to_port(to.port());
    std::memcpy(packet->template data<void>(), data, size);
    
    // Send the packet
    int result = _nic->send(buf);
    db<Protocol>(INF) << "[Protocol] NIC::send() returned value " << std::to_string(result) << "\n";
    
    // NIC::send will handle freeing the buffer now, so we don't need to free it
    // This avoids double-freeing if the NIC has taken ownership
    // _nic->free(buf);
    
    return size;
}

template <typename NIC>
int Protocol<NIC>::receive(Buffer* buf, Address from, void* data, unsigned int size) {
    db<Protocol>(TRC) << "Protocol<NIC>::receive() called!\n";
    
    // Check if protocol is still active
    if (!active()) {
        db<Protocol>(WRN) << "[Protocol] receive() called while protocol is inactive\n";
        if (buf) _nic->free(buf);
        return 0;
    }

    if (!buf) {
        db<Protocol>(WRN) << "[Protocol] receive() called with null buffer\n";
        return 0;
    }

    typename NIC::Address src_mac;
    typename NIC::Address dst_mac;

    std::uint8_t temp_buffer[size];
    
    int packet_size = _nic->receive(buf, &src_mac, &dst_mac, temp_buffer, NIC::MTU);
    db<Protocol>(INF) << "[Protocol] NIC::receive returned size " << std::to_string(packet_size) << "\n";

    if (packet_size < static_cast<int>(sizeof(Header))) {
        db<Protocol>(ERR) << "[Protocol] Packet size too small\n";
        return 0;
    }

    // Interpretar como Packet
    Packet* pkt = reinterpret_cast<Packet*>(temp_buffer);

    if (from) {
        from.paddr(src_mac);
        from.port(pkt->header()->from_port());
    }

    // Calcular tamanho do payload
    int payload_size = packet_size - sizeof(Header);

    // Copiar apenas os dados úteis para o usuário
    std::memcpy(data, pkt->template data<void>(), payload_size);

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
void Protocol<NIC>::update(typename NIC::Protocol_Number prot, Buffer * buf) {
    db<Protocol>(TRC) << "Protocol<NIC>::update() called!\n";
    
    // Check if protocol is active before processing
    if (!active()) {
        db<Protocol>(WRN) << "[Protocol] update() called while protocol is inactive, freeing buffer\n";
        if (buf) _nic->free(buf);
        return;
    }
    
    // Validate buffer before processing
    if (!buf || !buf->data()) {
        db<Protocol>(ERR) << "[Protocol] update() called with invalid buffer\n";
        return;
    }

    // Extracting packet from buffer
    Packet* packet = reinterpret_cast<Packet*>(buf->data()->payload);
    
    // Extracting dst port from packet
    Port dst_port = packet->header()->to_port();

    // If we have observers, notify them and let reference counting handle buffer cleanup
    // Otherwise, free the buffer ourselves
    if (!Protocol::_observed.notify(dst_port, buf)) { // Use port for notification
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