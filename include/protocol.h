#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <string>
#include <iostream>
#include <cstring>
#include <atomic>
#include <sstream>
#include <iomanip> // For std::setfill, std::setw, std::hex

#include "nic.h"
#include "traits.h"
#include "debug.h"
#include "observed.h"
#include "ethernet.h" // Assuming Ethernet namespace is included

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

                std::string to_string() const;

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

        static void attach(Observer* obs, Address address);
        static void detach(Observer* obs, Address address);
        
        // Add buffer free method
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
std::string Protocol<NIC>::Address::to_string() const {
    std::stringstream ss;
    std::string mac_str;
    try {
        // Use the utility function from Ethernet namespace (likely in ethernet.h)
        mac_str = Ethernet::mac_to_string(_paddr);
        // Check if the conversion resulted in an empty string
        if (mac_str.empty()) {
            mac_str = "[MAC:?]"; // Use placeholder if MAC string is empty
        }
    } catch (...) {
        // Fallback if Ethernet::mac_to_string throws an exception
        mac_str = "[MAC:ERR]"; // Use different placeholder for exception case
    }
    ss << mac_str << ":" << _port; // Combine MAC string and port
    return ss.str();
}

/********* Protocol Implementation *********/
template <typename NIC>
Protocol<NIC>::Protocol(NIC* nic) : NIC::Observer(PROTO), _nic(nic) {
    db<Protocol>(TRC) << "Protocol<NIC>::Protocol() called!\n";
    
    if (!nic) {
        throw std::invalid_argument("NIC pointer cannot be null");
    }

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

    db<Protocol>(INF) << "[Protocol] sending from port " << from.port() << " to port " << to.port() << "\n";

    // Calculate total size needed for the *entire* Ethernet frame
    unsigned int total_frame_size = Ethernet::HEADER_SIZE + sizeof(Header) + size;

    // Check against MTU before allocation (optional but good practice)
    if (total_frame_size > NIC::MTU + Ethernet::HEADER_SIZE) { // Compare total size with NIC capacity
         db<Protocol>(ERR) << "[Protocol] Data size (" << size << ") + Proto Header (" << sizeof(Header) << ") exceeds MTU (" << NIC::MTU << "). Dropping packet.\n";
         return 0; // Indicate failure due to size
    }

    // Allocate buffer for the entire frame
    // NIC::alloc expects the total frame size
    Buffer* buf = _nic->alloc(to.paddr(), PROTO, total_frame_size);
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
    db<Protocol>(INF) << "[Protocol] NIC::send() returned value " << std::to_string(result) << "\n";

    // --- Buffer Freeing on Failure ---
    if (result <= 0) {
        db<Protocol>(ERR) << "[Protocol] NIC::send failed. Freeing allocated buffer.\n";
        _nic->free(buf); // Free the buffer if send failed
        return 0; // Indicate send failure
    }
    // --- End Buffer Freeing ---

    // If send was successful, NIC/Engine handles the buffer from here on.
    // Protocol layer doesn't free it on success.
    // Return the size of the *user data* sent successfully.
    return size;
}

template <typename NIC>
int Protocol<NIC>::receive(Buffer* buf, Address *from, void* data, unsigned int size) {
    db<Protocol>(TRC) << "Protocol<NIC>::receive() called!\n";

    typename NIC::Address src_mac;
    typename NIC::Address dst_mac;

    std::uint8_t temp_buffer[size];
    
    int packet_size = _nic->receive(buf, &src_mac, &dst_mac, temp_buffer, NIC::MTU);
    db<Protocol>(INF) << "[Protocol] NIC::receive returned size " << std::to_string(packet_size) << "\n";

    if (packet_size < static_cast<int>(sizeof(Header))) {
        db<Protocol>(ERR) << "[Protocol] Packet size too small\n";
        return 0;
    }

    db<Protocol>(INF) << "[Protocol] Received packet from " << Ethernet::mac_to_string(src_mac) << " to " << Ethernet::mac_to_string(dst_mac) << "\n";
    db<Protocol>(INF) << "[Protocol] Packet size: " << packet_size << "\n";

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
    
    // Extracting Ethernet Frame to check source MAC
    Ethernet::Frame* frame = buf->data();
    Ethernet::Address src_mac = frame->src;
    Ethernet::Address my_mac = _nic->address(); // Get NIC's MAC address

    // Extracting packet from buffer payload
    Packet* packet = reinterpret_cast<Packet*>(frame->payload);
    
    // Extracting dst port from packet header
    Port dst_port = packet->header()->to_port();

    // --- Security Check: Drop external packets to broadcast port 0 --- 
    if (src_mac != my_mac && dst_port == 0) {
        db<Protocol>(WRN) << "[Protocol] Dropping external packet (src=" << Ethernet::mac_to_string(src_mac) << ") destined for broadcast port 0.\n";
        _nic->free(buf); // Free the buffer
        return;          // Do not process further
    }
    // ------------------------------------------------------------------

    // If we have observers, notify them based on destination port (broadcast handled by notify)
    // Let reference counting handle buffer cleanup if notified.
    // Otherwise, free the buffer ourselves
    if (!Protocol::_observed.notify(dst_port, buf)) { // Use port for notification
        db<Protocol>(INF) << "[Protocol] data received, but no one was notified for port " << dst_port << ". Freeing buffer.\n";
        // No observers for this specific port (and not broadcast, or broadcast had no observers)
        _nic->free(buf);
    }
}

// Add implementation for Protocol::free
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
        Protocol<NIC>::Physical_Address({0xff, 0xff, 0xff, 0xff, 0xff, 0xff}), // MAC broadcast
        Protocol<NIC>::Address::NULL_VALUE // ou um valor reservado, como "porta broadcast"
    );
#endif // PROTOCOL_H