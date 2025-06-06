#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <string>
#include <cstring>
#include <chrono>
#include <cstdint>
#include <cstddef> // Ensure this is included for offsetof
#include <stdexcept> // Ensure this is included for std::invalid_argument

#include "api/traits.h"
#include "api/util/debug.h"
#include "api/util/observed.h"
#include "api/util/observer.h"
#include "api/network/ethernet.h"
#include "api/framework/clock.h"  // Include Clock for timestamping
#include "api/framework/location_service.h"  // Include LocationService
#include "api/util/geo_utils.h"  // Include GeoUtils

// Forward declaration to avoid circular dependency
template <typename Protocol_T> class VehicleRSUManager;

// Protocol implementation that works with the real Communicator
template <typename NIC>
class Protocol: private NIC::Observer
{
    public:
        static const typename NIC::Protocol_Number PROTO = Traits<Protocol>::ETHERNET_PROTOCOL_NUMBER;
        
        typedef typename NIC::DataBuffer Buffer;
        typedef typename NIC::Address Physical_Address;
        typedef std::uint16_t Port;
        
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
        
        // Timestamp fields structure for NIC-level timestamping
        struct TimestampFields {
            bool is_clock_synchronized;      // Clock synchronization status (filled by Protocol on send)
            TimestampType tx_timestamp;      // Filled by NIC on send
            
            TimestampFields() : 
                is_clock_synchronized(false),
                tx_timestamp(TimestampType::min()) {}
        };
        
        static const unsigned int MTU = NIC::MTU - sizeof(Header) - sizeof(TimestampFields) - sizeof(Coordinates);
        typedef std::uint8_t Data[MTU];
        
        // Packet class that includes header, timestamp fields, coordinates, and data
        class Packet: public Header {
            public:
                Packet() {}
                
                Header* header() { return this; }

                TimestampFields* timestamps() { 
                    return reinterpret_cast<TimestampFields*>(
                        reinterpret_cast<uint8_t*>(this) + sizeof(Header)
                    ); 
                }
                
                Coordinates* coordinates() {
                    return reinterpret_cast<Coordinates*>(
                        reinterpret_cast<uint8_t*>(this) + sizeof(Header) + sizeof(TimestampFields)
                    );
                }
                
                template<typename T>
                T* data() { 
                    return reinterpret_cast<T*>(
                        reinterpret_cast<uint8_t*>(this) + sizeof(Header) + sizeof(TimestampFields) + sizeof(Coordinates)
                    ); 
                }
                
                // Calculate offset to timestamp fields from start of packet
                static constexpr unsigned int sync_status_offset() {
                    return sizeof(Header) + offsetof(TimestampFields, is_clock_synchronized);
                }
                
                static constexpr unsigned int tx_timestamp_offset() {
                    return sizeof(Header) + offsetof(TimestampFields, tx_timestamp);
                }
                
            private:
                Data _data;
                // Note: Actual timestamp fields, coordinates, and data are accessed via pointers
                // to maintain proper memory layout
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
        
        enum class EntityType { VEHICLE, RSU, UNKNOWN };
        
        // Modified constructor
        Protocol(NIC* nic, EntityType entity_type = EntityType::UNKNOWN);
        
        // New method to set entity manager (only for vehicles)
        void set_vehicle_rsu_manager(VehicleRSUManager<Protocol>* manager);

        ~Protocol();
        
        int send(Address from, Address to, const void* data, unsigned int size);
        int receive(Buffer* buf, Address *from, void* data, unsigned int size);

        // Method to free a buffer, crucial for Communicator to prevent leaks
        void free(Buffer* buf);

        static void attach(Observer* obs, Address address);
        static void detach(Observer* obs, Address address);

    private:
        void update(typename NIC::Protocol_Number prot, Buffer * buf) override;
        void handle_status_message(const Message<Protocol>& status_msg,
                                  const Coordinates* sender_coords,
                                  const TimestampFields* timestamps,
                                  const typename NIC::Address& sender_mac);

    private:
        NIC* _nic;
        static Observed _observed;
        EntityType _entity_type;
        VehicleRSUManager<Protocol>* _vehicle_rsu_manager; // nullptr for RSUs
};

/******** Protocol::Address Implementation ******/
template <typename NIC>
Protocol<NIC>::Address::Address() : _port(0), _paddr(NIC::NULL_ADDRESS) {}

template <typename NIC>
Protocol<NIC>::Address::Address(const Null& null) : _port(0), _paddr(NIC::NULL_ADDRESS) {}

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
Protocol<NIC>::Protocol(NIC* nic, EntityType entity_type)
    : NIC::Observer(PROTO), _nic(nic), _entity_type(entity_type), _vehicle_rsu_manager(nullptr) {
    if (!nic) {
        throw std::invalid_argument("NIC pointer cannot be null");
    }
    _nic->attach(this, PROTO);
    db<Protocol>(INF) << "[Protocol] created for "
                      << (entity_type == EntityType::VEHICLE ? "VEHICLE" : 
                          entity_type == EntityType::RSU ? "RSU" : "UNKNOWN") << "\n";
}

template <typename NIC>
Protocol<NIC>::~Protocol() {
    _nic->detach(this, PROTO);
    db<Protocol>(INF) << "[Protocol] detached from NIC\n";
}

template <typename NIC>
int Protocol<NIC>::send(Address from, Address to, const void* data, unsigned int size) {
    db<Protocol>(TRC) << "Protocol<NIC>::send() called!\n";

    if (!_nic) {
        db<Protocol>(TRC) << "Protocol<NIC>::send() called after release!\n";
        return 0;
    }

    // Allocate buffer for the entire frame -> NIC alloc adds Frame Header size (this is better for independency)
    unsigned int packet_size = size + sizeof(Header) + sizeof(TimestampFields) + sizeof(Coordinates);
    Buffer* buf = _nic->alloc(to.paddr(), PROTO, packet_size);

    // Get pointer to where Protocol packet starts (within the Ethernet payload)
    // Assumes buf->data() returns Ethernet::Frame* and payload follows header
    Packet* packet = reinterpret_cast<Packet*>(buf->data()->payload);

    // Set up Protocol Packet Header
    packet->from_port(from.port());
    packet->to_port(to.port());
    packet->size(size); // Set the size of the *user data*

    // Copy user data into the packet's data section
    std::memcpy(packet->template data<void>(), data, size);

    // Set clock synchronization status in the packet
    auto& clock = Clock::getInstance();
    bool sync_status;
    clock.getSynchronizedTime(&sync_status); // We only need the status
    packet->timestamps()->is_clock_synchronized = sync_status;

    // Set sender location and communication radius
    Coordinates coords;
    coords.radius = _nic->radius();
    LocationService::getCurrentCoordinates(coords.latitude, coords.longitude);
    std::memcpy(packet->coordinates(), &coords, sizeof(Coordinates));

    // Send the packet via NIC, passing the packet size for timestamp offset calculation
    int result = _nic->send(buf, packet_size); // Pass packet size to NIC
    db<Protocol>(INF) << "[Protocol] NIC::send() returned " << result << ", clock_synchronized=" << packet->timestamps()->is_clock_synchronized << "\n";
    
    // NIC should release buffer after use
    return result;
}

template <typename NIC>
int Protocol<NIC>::receive(Buffer* buf, Address *from, void* data, unsigned int size) {
    db<Protocol>(TRC) << "Protocol<NIC>::receive() called!\n";

    typename NIC::Address src_mac;
    typename NIC::Address dst_mac;

    std::uint8_t temp_buffer[size + sizeof(Header) + sizeof(TimestampFields) + sizeof(Coordinates)];
    
    if (!_nic) {
        db<Protocol>(TRC) << "Protocol<NIC>::receive() called after release!\n";
        return -1;
    }
    
    int packet_size = _nic->receive(buf, &src_mac, &dst_mac, temp_buffer, NIC::MTU);
    db<Protocol>(INF) << "[Protocol] NIC::receive() returned " << packet_size << ".\n";

    if (packet_size <= 0) {
        db<Protocol>(WRN) << "[Protocol] No data received or error occurred.\n";
        return -1; // No data received or error
    }
    
    // Reinterpretation as packet
    Packet* pkt = reinterpret_cast<Packet*>(temp_buffer);

    if (from) {
        from->paddr(src_mac);
        from->port(pkt->header()->from_port());
    }

    // Payload size (accounting for Coordinates)
    int payload_size = packet_size - sizeof(Header) - sizeof(TimestampFields) - sizeof(Coordinates);

    // Copies only useful data
    std::memcpy(data, pkt->template data<void>(), payload_size);

    db<Protocol>(INF) << "[Protocol] received packet from " << Ethernet::mac_to_string(src_mac) << " to " << Ethernet::mac_to_string(dst_mac) << " with size " << packet_size << "\n";

    return payload_size;
}

template <typename NIC>
void Protocol<NIC>::attach(Observer* obs, Address address) {    
    _observed.attach(obs, address.port());
    db<Protocol>(INF) << "[Protocol] Attached observer to port " << address.port() << "\n";
}

template <typename NIC>
void Protocol<NIC>::detach(Observer* obs, Address address) {
    _observed.detach(obs, address.port());
    db<Protocol>(INF) << "[Protocol] Detached observer from port " << address.port() << "\n";
}

template <typename NIC>
void Protocol<NIC>::update(typename NIC::Protocol_Number prot, Buffer * buf) {
    db<Protocol>(TRC) << "Protocol<NIC>::update() called!\n";

    if (!buf) {
        db<Protocol>(INF) << "[Protocol] data received, but buffer is null. Releasing buffer.\n";
        return;
    }

    Packet* pkt = reinterpret_cast<Packet*>(buf->data()->payload);

    // Radius-based collision domain filtering
    Coordinates* coords = pkt->coordinates();
    // Get receiver location
    double rx_lat, rx_lon;
    LocationService::getCurrentCoordinates(rx_lat, rx_lon);
    // Check if packet is within sender's communication range
    double distance = GeoUtils::haversineDistance(coords->latitude, coords->longitude, rx_lat, rx_lon);
    if (distance > coords->radius) {
        db<Protocol>(INF) << "[Protocol] Packet dropped: out of range (" << distance << "m > " << coords->radius << "m)\n";
        free(buf);
        return;
    }

    // Extract timestamps and update Clock if this is a PTP-relevant message
    TimestampFields* timestamps = pkt->timestamps();
    db<Protocol>(INF) << "[Protocol] Received packet with sender_clock_synchronized=" 
                      << timestamps->is_clock_synchronized << "\n";
    TimestampType rx_timestamp(std::chrono::microseconds(buf->rx()));
    typename NIC::Address src_mac = buf->data()->src;
    PtpRelevantData ptp_data;
    ptp_data.sender_id = static_cast<LeaderIdType>(src_mac.bytes[5]);
    ptp_data.ts_tx_at_sender = timestamps->tx_timestamp;
    ptp_data.ts_local_rx = rx_timestamp;
    auto& clock = Clock::getInstance();
    clock.activate(&ptp_data);

    // NEW: STATUS message interception
    int payload_size = buf->size() - sizeof(Header) - sizeof(TimestampFields) - sizeof(Coordinates);
    if (payload_size > 0) {
        const uint8_t* message_data = pkt->template data<uint8_t>();
        if (payload_size >= 1) {
            uint8_t raw_msg_type = message_data[0];
            if (raw_msg_type == static_cast<uint8_t>(Message<Protocol>::Type::STATUS)) {
                db<Protocol>(INF) << "[Protocol] Intercepted STATUS message\n";
                Message<Protocol> status_msg = Message<Protocol>::deserialize(message_data, payload_size);
                if (status_msg.message_type() == Message<Protocol>::Type::STATUS) {
                    handle_status_message(status_msg, pkt->coordinates(), pkt->timestamps(), buf->data()->src);
                }
                free(buf);
                return;
            }
        }
    }

    // For non-STATUS messages, continue normal flow
    if (!Protocol::_observed.notify(buf)) {
        db<Protocol>(INF) << "[Protocol] data received, but no one was notified for port. Releasing buffer.\n";
        free(buf);
    }
    db<Protocol>(INF) << "[Protocol] data received, notify succeeded.\n";
}

// STATUS message handler

template <typename NIC>
void Protocol<NIC>::handle_status_message(const Message<Protocol>& status_msg,
                                         const Coordinates* sender_coords,
                                         const TimestampFields* timestamps,
                                         const typename NIC::Address& sender_mac) {
    db<Protocol>(INF) << "[Protocol] Processing STATUS message from " 
                      << Ethernet::mac_to_string(sender_mac) << "\n";
    // Only vehicles process STATUS messages from RSUs
    if (_entity_type != EntityType::VEHICLE || !_vehicle_rsu_manager) {
        db<Protocol>(INF) << "[Protocol] Ignoring STATUS message (not a vehicle or no RSU manager)\n";
        return;
    }
    
    // Extract RSU information from STATUS message payload
    const uint8_t* payload = status_msg.value();
    unsigned int payload_size = status_msg.value_size();
    if (payload_size < (sizeof(double) * 3 + sizeof(MacKeyType))) {
        db<Protocol>(WRN) << "[Protocol] STATUS message payload too small: " << payload_size << "\n";
        return;
    }
    
    // Parse payload
    unsigned int offset = 0;
    double rsu_lat, rsu_lon, rsu_radius;
    MacKeyType rsu_key;
    std::memcpy(&rsu_lat, payload + offset, sizeof(double));
    offset += sizeof(double);
    std::memcpy(&rsu_lon, payload + offset, sizeof(double));
    offset += sizeof(double);
    std::memcpy(&rsu_radius, payload + offset, sizeof(double));
    offset += sizeof(double);
    std::memcpy(&rsu_key, payload + offset, sizeof(MacKeyType));
    
    // Create Protocol address for the RSU
    Address rsu_address(sender_mac, status_msg.origin().port());
    
    // Forward to RSU manager - this will be linked at compile time when VehicleRSUManager is fully defined
    if (_vehicle_rsu_manager) {
        _vehicle_rsu_manager->process_rsu_status(rsu_address, rsu_lat, rsu_lon, rsu_radius, rsu_key);
        db<Protocol>(INF) << "[Protocol] Forwarded RSU info to manager: lat=" << rsu_lat 
                          << ", lon=" << rsu_lon << ", radius=" << rsu_radius << "\n";
    }
}

// Implementation for the new free method
template <typename NIC>
void Protocol<NIC>::free(Buffer* buf) {
    if (_nic) {
        _nic->free(buf);
    }
}

// Initialize static members
template <typename NIC>
typename Protocol<NIC>::Observed Protocol<NIC>::_observed;

// Initialize BROADCASTs addresses
template <typename NIC>
const typename Protocol<NIC>::Address Protocol<NIC>::Address::BROADCAST = 
    typename Protocol<NIC>::Address(
        Ethernet::BROADCAST, // MAC broadcast
        0 // Broadcast port
    );

template <typename NIC>
void Protocol<NIC>::set_vehicle_rsu_manager(VehicleRSUManager<Protocol>* manager) {
    if (_entity_type == EntityType::VEHICLE) {
        _vehicle_rsu_manager = manager;
        db<Protocol>(INF) << "[Protocol] RSU manager attached to vehicle protocol\n";
    } else {
        db<Protocol>(WRN) << "[Protocol] Attempted to attach RSU manager to non-vehicle entity\n";
    }
}

#endif // PROTOCOL_H