#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <string>
#include <cstring>
#include <atomic>
#include <ostream>

#include "traits.h"
#include "debug.h"
#include "observed.h"
#include "observer.h"
#include "ethernet.h"

// Protocol implementation that works with the real Communicator
template <typename NIC>
class Protocol: private NIC::Observer
{
    public:
        static const typename NIC::Protocol_Number PROTO = Traits<Protocol>::ETHERNET_PROTOCOL_NUMBER;
        static const unsigned int MTU = NIC::MTU - Ethernet::HEADER_SIZE;
        
        typedef typename NIC::DataBuffer Buffer;
        typedef typename NIC::Address Physical_Address;
        typedef std::uint16_t Port;
        typedef Conditional_Data_Observer<Buffer, Port> Observer;
        typedef Conditionally_Data_Observed<Buffer, Port> Observed;
        typedef std::uint8_t Data[MTU];
        
        // Port constants for the P3 implementation
        inline static const Port GATEWAY_PORT = 0;
        inline static const Port INTERNAL_BROADCAST_PORT = 1;
        inline static const Port MIN_COMPONENT_PORT = 2;
        
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
                T* data() { return reinterpret_cast<T*>(_data); }
                
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
                const std::string to_string() const;
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
        int receive(Buffer* buf, Address *from, void* data, unsigned int size);
        int peek(Buffer* buf, void* data, unsigned int size);
        static void attach(Observer* obs, Address address);
        static void detach(Observer* obs, Address address);
        void free(Buffer* buf);

    private:
        void update(typename NIC::Protocol_Number prot, Buffer * buf) override;

    private:
        NIC* _nic;
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

template <typename NIC>
const std::string Protocol<NIC>::Address::to_string() const {
    std::string mac_addr = NIC::mac_to_string(_paddr);
    return mac_addr + ":" + std::to_string(_port);
}

/********* Protocol Implementation *********/
template <typename NIC>
Protocol<NIC>::Protocol(NIC* nic) : NIC::Observer(PROTO), _nic(nic) {
    db<Protocol>(TRC) << "[Protocol] Constructor called!\n";
    
    if (!nic) {
        throw std::invalid_argument("NIC pointer cannot be null");
    }

    _nic->attach(this, PROTO);
    db<Protocol>(INF) << "[Protocol] attached to NIC\n";
}

template <typename NIC>
Protocol<NIC>::~Protocol() {
    db<Protocol>(TRC) << "[Protocol] Destructor called!\n";
    
    _nic->detach(this, PROTO);
    db<Protocol>(INF) << "[Protocol] detached from NIC\n";
}

template <typename NIC>
int Protocol<NIC>::send(Address from, Address to, const void* data, unsigned int size) {
    db<Protocol>(TRC) << "[Protocol] send() called!\n";

    db<Protocol>(INF) << "[Protocol] sending from " << from.to_string() << " to " << to.to_string() << "\n";

    // Allocate buffer for the entire frame -> NIC alloc adds Frame Header size (this is better for independency)
    unsigned int packet_size = size + sizeof(Header);
    Buffer* buf = _nic->alloc(to.paddr(), PROTO, packet_size);
    if (!buf) {
        db<Protocol>(ERR) << "[Protocol] Failed to allocate buffer for send\n";
        return 0; // Indicate failure, no buffer allocated
    }

    // Get pointer to where Protocol packet starts (within the Ethernet payload)
    // Assumes buf->data() returns Ethernet::Frame* and payload follows header
    Packet* packet = reinterpret_cast<Packet*>(buf->data()->payload);

    // Set up Protocol Packet Header
    packet->from_port(from.port());
    packet->to_port(to.port());
    packet->size(size); // Set the size of the *user data*

    // Copy user data into the packet's data section
    std::memcpy(packet->template data<void>(), data, size);

    // Send the packet via NIC
    int result = _nic->send(buf); // NIC::send takes the buffer containing the full frame

    // --- Buffer Release on Failure ---
    if (result <= 0) {
        db<Protocol>(ERR) << "[Protocol] failed to send message.\n";
    } else {
        db<Protocol>(INF) << "[Protocol] message sucessfully sent.\n";
    }

    // NIC should release buffer after use
    
    return result;
}

template <typename NIC>
int Protocol<NIC>::receive(Buffer* buf, Address *from, void* data, unsigned int size) {
    db<Protocol>(TRC) << "[Protocol] receive() called!\n";

    typename NIC::Address src_mac;
    typename NIC::Address dst_mac;

    std::uint8_t temp_buffer[size];
    
    int packet_size = _nic->receive(buf, &src_mac, &dst_mac, temp_buffer, NIC::MTU);
    db<Protocol>(INF) << "[Protocol] NIC::send() returned " << packet_size << ".\n";

    if (packet_size < 0) {
        db<Protocol>(ERR) << "[Protocol] failed to receive message.\n";
        return -1;
    }

    if (packet_size < static_cast<int>(sizeof(Header))) {
        db<Protocol>(ERR) << "[Protocol] received undersized packet.\n";
        return -1;
    }

    db<Protocol>(INF) << "[Protocol] received packet from " << Ethernet::mac_to_string(src_mac) << " to " << Ethernet::mac_to_string(dst_mac) << " with size " << packet_size << "\n";

    // Interpretar como Packet
    Packet* pkt = reinterpret_cast<Packet*>(temp_buffer);

    if (from) {
        from->paddr(src_mac);
        from->port(pkt->header()->from_port());
    } else {
        db<Protocol>(ERR) << "[Protocol] from is null\n";
    }

    // Calcular tamanho do payload
    int payload_size = packet_size - sizeof(Header);

    // Copiar apenas os dados úteis para o usuário
    std::memcpy(data, pkt->template data<void>(), payload_size);

    return payload_size;
}

// Peek method to read data from a buffer without consuming it
template <typename NIC>
int Protocol<NIC>::peek(Buffer* buf, void* data, unsigned int size) {
    db<Protocol>(TRC) << "[Protocol] peek() called!\n";
    
    if (!buf || !data) {
        db<Protocol>(ERR) << "[Protocol] peek received null buffer or data pointer\n";
        return -1;
    }
    
    // Get pointer to the packet within the Ethernet frame's payload
    Packet* packet = reinterpret_cast<Packet*>(buf->data()->payload);
    
    // Calculate available data size (limited by packet size and requested size)
    unsigned int available_size = std::min(packet->size(), size);
    
    // Copy data to the provided buffer without modifying the original
    std::memcpy(data, packet->template data<void>(), available_size);
    
    db<Protocol>(INF) << "[Protocol] peeked " << available_size << " bytes from buffer\n";
    
    return available_size;
}

template <typename NIC>
void Protocol<NIC>::attach(Observer* obs, Address address) {
    db<Protocol>(TRC) << "[Protocol] attach() called!\n";
    
    _observed.attach(obs, address.port());
    db<Protocol>(INF) << "[Protocol] Attached observer to port " << address.port() << "\n";
}

template <typename NIC>
void Protocol<NIC>::detach(Observer* obs, Address address) {
    db<Protocol>(TRC) << "[Protocol] detach() called!\n";

    _observed.detach(obs, address.port());
    db<Protocol>(INF) << "[Protocol] Detached observer from port " << address.port() << "\n";
}

template <typename NIC>
void Protocol<NIC>::update(typename NIC::Protocol_Number prot, Buffer * buf) {
    db<Protocol>(TRC) << "[Protocol] update() called!\n";
    
    // Extracting MAC Addresses to compare
    Physical_Address src_mac = buf->data()->src;

    // Extracting packet from frame payload
    Packet* packet = reinterpret_cast<Packet*>(buf->data()->payload);
    
    // Extract source and destination ports from packet header
    Port src_port = packet->header()->from_port();
    Port dst_port = packet->header()->to_port();

    db<Protocol>(INF) << "[Protocol] Received packet from src=" << Ethernet::mac_to_string(src_mac) 
                     << ":" << src_port << " to dst_port=" << dst_port << "\n";

    // ------------------------------------------------------------------
    // GATEWAY_PORT HANDLING - This is for messages sent to the gateway from outside
    if (dst_port == GATEWAY_PORT) {
        db<Protocol>(INF) << "[Protocol] Received packet on GATEWAY_PORT\n";
        // Simply notify observers of Gateway Port (GatewayComponent)
        if (!_observed.notify(GATEWAY_PORT, buf)) {
            db<Protocol>(INF) << "[Protocol] No observer found for GATEWAY_PORT. Freeing buffer.\n";
            _nic->free(buf);
        }
    }
    // INTERNAL_BROADCAST_PORT HANDLING - This is for internal broadcast within the vehicle
    else if (dst_port == INTERNAL_BROADCAST_PORT) {
        db<Protocol>(INF) << "[Protocol] Received packet for INTERNAL_BROADCAST_PORT\n";
        
        // Create a buffer cloning lambda function
        auto clone_buffer_func = [this, prot](Buffer* original) -> Buffer* {
            if (!original) return nullptr;
            
            // Allocate a new buffer with same size as original, minus Ethernet header
            Buffer* cloned_buf = _nic->alloc(original->data()->dst, prot, original->size() - Ethernet::HEADER_SIZE);
            if (!cloned_buf) {
                db<Protocol>(ERR) << "[Protocol] Failed to allocate buffer for internal broadcast\n";
                return nullptr;
            }
            
            // Copy data from original to clone
            std::memcpy(cloned_buf->data(), original->data(), original->size());
            return cloned_buf;
        };

        db<Protocol>(INF) << "[Protocol] Broadcasting to all observers on INTERNAL_BROADCAST_PORT\n";
        bool t = _observed.notifyInternalBroadcast(buf, INTERNAL_BROADCAST_PORT, src_port, clone_buffer_func);
        db<Protocol>(INF) << "[Protocol] Finished notifying observers for INTERNAL_BROADCAST_PORT\n";
        // Use the specialized internal broadcast method which handles buffer cloning
        if (!t) {
            db<Protocol>(INF) << "[Protocol] No observers notified for INTERNAL_BROADCAST_PORT. Freeing buffer.\n";
            _nic->free(buf);
        }
        db<Protocol>(INF) << "[Protocol] Finished notifying observers for INTERNAL_BROADCAST_PORT\n";
    }
    // COMPONENT PORT HANDLING (specific port >= MIN_COMPONENT_PORT)
    else if (dst_port >= MIN_COMPONENT_PORT) {
        db<Protocol>(INF) << "[Protocol] Received packet for component port " << dst_port << "\n";
        
        // Notify specific component observer
        if (!_observed.notify(dst_port, buf)) {
            db<Protocol>(INF) << "[Protocol] No observer found for port " << dst_port << ". Freeing buffer.\n";
            _nic->free(buf);
        }
    }
    // Unrecognized port (should not happen in normal operation)
    else {
        db<Protocol>(WRN) << "[Protocol] Received packet with unrecognized destination port " << dst_port << "\n";
        _nic->free(buf);
    }
    db<Protocol>(INF) << "[Protocol] update() completed.\n";
}

template <typename NIC>
void Protocol<NIC>::free(Buffer* buf) {
    if (_nic) {
        _nic->free(buf);
    } else {
        // Log error or handle case where NIC pointer is null (shouldn't happen in normal operation)
        db<Protocol>(ERR) << "[Protocol] free() called but _nic is null!\n";
    }
}

// Initialize static members
template <typename NIC>
typename Protocol<NIC>::Observed Protocol<NIC>::_observed;

// Initialize the BROADCAST address
template <typename NIC>
const typename Protocol<NIC>::Address Protocol<NIC>::Address::BROADCAST = 
    typename Protocol<NIC>::Address(
        Ethernet::BROADCAST, // MAC broadcast
        0 // Broadcast port
    );
#endif // PROTOCOL_H