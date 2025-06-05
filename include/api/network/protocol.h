#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <string>
#include <cstring>
#include <chrono>
#include <cstdint>
#include <cstddef> // Ensure this is included for offsetof
#include <stdexcept> // Ensure this is included for std::invalid_argument
#include <cstdio> // For snprintf in debug logging

#include "api/traits.h"
#include "api/util/debug.h"
#include "api/util/observed.h"
#include "api/util/observer.h"
#include "api/network/ethernet.h"
#include "api/framework/clock.h"  // Include Clock for timestamping
#include "api/framework/location_service.h"  // Include LocationService
#include "api/util/geo_utils.h"  // Include GeoUtils
#include "api/network/message.h"
#include "api/framework/leaderKeyStorage.h"

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
        typedef Message<Protocol<NIC>> ProtocolMessage;
        
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
        
        // MAC authentication field structure
        struct AuthenticationFields {
            MacKeyType mac;                  // Message Authentication Code for INTEREST/RESPONSE messages
            bool has_mac;                    // Flag indicating if MAC is present and valid
            
            AuthenticationFields() : has_mac(false) {
                mac.fill(0);
            }
        };
        
        static const unsigned int MTU = NIC::MTU - sizeof(Header) - sizeof(TimestampFields) - sizeof(Coordinates) - sizeof(AuthenticationFields);
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
                
                AuthenticationFields* authentication() {
                    return reinterpret_cast<AuthenticationFields*>(
                        reinterpret_cast<uint8_t*>(this) + sizeof(Header) + sizeof(TimestampFields) + sizeof(Coordinates)
                    );
                }
                
                template<typename T>
                T* data() { 
                    return reinterpret_cast<T*>(
                        reinterpret_cast<uint8_t*>(this) + sizeof(Header) + sizeof(TimestampFields) + sizeof(Coordinates) + sizeof(AuthenticationFields)
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
        void handle_status_message(const typename ProtocolMessage::Type& msg_type,
                                  const uint8_t* message_data, unsigned int payload_size,
                                  const Coordinates* sender_coords,
                                  const TimestampFields* timestamps,
                                  const typename NIC::Address& sender_mac);
        
        // MAC authentication methods
        MacKeyType calculate_mac(const void* data, unsigned int size, const MacKeyType& key);
        bool verify_mac(const void* data, unsigned int size, const MacKeyType& received_mac);
        bool requires_authentication(const void* data, unsigned int size);
        bool is_authenticated_message_type(typename ProtocolMessage::Type type);

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
    unsigned int packet_size = size + sizeof(Header) + sizeof(TimestampFields) + sizeof(Coordinates) + sizeof(AuthenticationFields);
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

    // Initialize authentication fields
    packet->authentication()->has_mac = false;
    packet->authentication()->mac.fill(0);

    // Calculate MAC for INTEREST and RESPONSE messages
    if (requires_authentication(data, size)) {
        db<Protocol>(TRC) << "[Protocol] Calculating MAC for authenticated message\n";
        
        // Get leader key from storage
        MacKeyType leader_key = LeaderKeyStorage::getInstance().getGroupMacKey();
        
        // Debug logging: Show message data
        db<Protocol>(INF) << "[Protocol] MAC Auth - Message size: " << size << " bytes\n";
        const uint8_t* msg_bytes = static_cast<const uint8_t*>(data);
        std::string msg_hex = "";
        for (unsigned int i = 0; i < std::min(size, 32u); ++i) {  // Log first 32 bytes
            char hex_byte[4];
            snprintf(hex_byte, sizeof(hex_byte), "%02X ", msg_bytes[i]);
            msg_hex += hex_byte;
        }
        db<Protocol>(INF) << "[Protocol] MAC Auth - Message data (first " << std::min(size, 32u) << " bytes): " << msg_hex << "\n";
        
        // Debug logging: Show key
        std::string key_hex = "";
        for (size_t i = 0; i < 16; ++i) {
            char hex_byte[4];
            snprintf(hex_byte, sizeof(hex_byte), "%02X ", leader_key[i]);
            key_hex += hex_byte;
        }
        db<Protocol>(INF) << "[Protocol] MAC Auth - Key: " << key_hex << "\n";
        
        // Calculate MAC over the message payload
        MacKeyType calculated_mac = calculate_mac(data, size, leader_key);
        
        // Debug logging: Show calculated MAC
        std::string mac_hex = "";
        for (size_t i = 0; i < 16; ++i) {
            char hex_byte[4];
            snprintf(hex_byte, sizeof(hex_byte), "%02X ", calculated_mac[i]);
            mac_hex += hex_byte;
        }
        db<Protocol>(INF) << "[Protocol] MAC Auth - Calculated MAC: " << mac_hex << "\n";
        
        // Set MAC in packet
        packet->authentication()->mac = calculated_mac;
        packet->authentication()->has_mac = true;
        
        db<Protocol>(INF) << "[Protocol] Added MAC authentication to outgoing message\n";
    }

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

    std::uint8_t temp_buffer[size + sizeof(Header) + sizeof(TimestampFields) + sizeof(Coordinates) + sizeof(AuthenticationFields)];
    
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

    // Payload size (accounting for Coordinates and AuthenticationFields)
    int payload_size = packet_size - sizeof(Header) - sizeof(TimestampFields) - sizeof(Coordinates) - sizeof(AuthenticationFields);

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
    TimestampType rx_timestamp(std::chrono::milliseconds(buf->rx()));
    typename NIC::Address src_mac = buf->data()->src;
    PtpRelevantData ptp_data;
    ptp_data.sender_id = static_cast<LeaderIdType>(src_mac.bytes[5]);
    ptp_data.ts_tx_at_sender = timestamps->tx_timestamp;
    ptp_data.ts_local_rx = rx_timestamp;
    auto& clock = Clock::getInstance();
    clock.activate(&ptp_data);

    // Authentication verification for INTEREST and RESPONSE messages
    int payload_size = buf->size() - sizeof(Header) - sizeof(TimestampFields) - sizeof(Coordinates) - sizeof(AuthenticationFields);
    if (payload_size > 0) {
        const uint8_t* message_data = pkt->template data<uint8_t>();
        if (payload_size >= 1) {
            uint8_t raw_msg_type = message_data[0];
            
            // Check if this message type requires authentication
            if (raw_msg_type == static_cast<uint8_t>(ProtocolMessage::Type::INTEREST) ||
                raw_msg_type == static_cast<uint8_t>(ProtocolMessage::Type::RESPONSE)) {
                
                db<Protocol>(TRC) << "[Protocol] Verifying MAC for authenticated message\n";
                
                // Verify MAC authentication
                if (!verify_mac(message_data, payload_size, pkt->authentication()->mac)) {
                    db<Protocol>(WRN) << "[Protocol] MAC verification failed - dropping packet\n";
                    free(buf);
                    return;
                }
                
                db<Protocol>(INF) << "[Protocol] MAC verification successful\n";
            }
            
            // STATUS message interception (no authentication required)
            if (raw_msg_type == static_cast<uint8_t>(ProtocolMessage::Type::STATUS)) {
                db<Protocol>(INF) << "[Protocol] Intercepted STATUS message\n";
                auto msg_type = static_cast<typename ProtocolMessage::Type>(raw_msg_type);
                handle_status_message(msg_type, message_data, payload_size, pkt->coordinates(), pkt->timestamps(), buf->data()->src);
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
void Protocol<NIC>::handle_status_message(const typename ProtocolMessage::Type& msg_type,
                                         const uint8_t* message_data, unsigned int payload_size,
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
    
    // Deserialize the STATUS message to extract payload
    ProtocolMessage status_msg = ProtocolMessage::deserialize(message_data, payload_size);
    if (status_msg.message_type() != ProtocolMessage::Type::STATUS) {
        db<Protocol>(WRN) << "[Protocol] Failed to deserialize STATUS message\n";
        return;
    }
    
    // Extract RSU information from STATUS message payload
    const uint8_t* payload = status_msg.value();
    unsigned int value_size = status_msg.value_size();
    if (value_size < (sizeof(double) * 3 + sizeof(MacKeyType))) {
        db<Protocol>(WRN) << "[Protocol] STATUS message payload too small: " << value_size << "\n";
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

// MAC Authentication Implementation
template <typename NIC>
MacKeyType Protocol<NIC>::calculate_mac(const void* data, unsigned int size, const MacKeyType& key) {
    MacKeyType result;
    const uint8_t* message_bytes = static_cast<const uint8_t*>(data);
    
    // Simple XOR-based MAC calculation
    // Hash the message data first by XORing all bytes into 16-byte blocks
    result.fill(0);
    
    for (unsigned int i = 0; i < size; ++i) {
        result[i % 16] ^= message_bytes[i];
    }
    
    // XOR with the key
    for (size_t i = 0; i < 16; ++i) {
        result[i] ^= key[i];
    }
    
    return result;
}

template <typename NIC>
bool Protocol<NIC>::verify_mac(const void* data, unsigned int size, const MacKeyType& received_mac) {
    // Debug logging: Show received MAC
    std::string received_mac_hex = "";
    for (size_t i = 0; i < 16; ++i) {
        char hex_byte[4];
        snprintf(hex_byte, sizeof(hex_byte), "%02X ", received_mac[i]);
        received_mac_hex += hex_byte;
    }
    db<Protocol>(INF) << "[Protocol] MAC Verify - Received MAC: " << received_mac_hex << "\n";
    
    // Debug logging: Show message data being verified
    db<Protocol>(INF) << "[Protocol] MAC Verify - Message size: " << size << " bytes\n";
    const uint8_t* msg_bytes = static_cast<const uint8_t*>(data);
    std::string msg_hex = "";
    for (unsigned int i = 0; i < std::min(size, 32u); ++i) {  // Log first 32 bytes
        char hex_byte[4];
        snprintf(hex_byte, sizeof(hex_byte), "%02X ", msg_bytes[i]);
        msg_hex += hex_byte;
    }
    db<Protocol>(INF) << "[Protocol] MAC Verify - Message data (first " << std::min(size, 32u) << " bytes): " << msg_hex << "\n";
    
    // For vehicles: Check MAC against all known RSU keys
    if (_entity_type == EntityType::VEHICLE && _vehicle_rsu_manager) {
        auto known_rsus = _vehicle_rsu_manager->get_known_rsus();
        db<Protocol>(INF) << "[Protocol] MAC Verify - Vehicle checking against " << known_rsus.size() << " known RSU keys\n";
        
        for (size_t idx = 0; idx < known_rsus.size(); ++idx) {
            const auto& rsu = known_rsus[idx];
            
            // Debug logging: Show RSU key being tested
            std::string rsu_key_hex = "";
            for (size_t i = 0; i < 16; ++i) {
                char hex_byte[4];
                snprintf(hex_byte, sizeof(hex_byte), "%02X ", rsu.group_key[i]);
                rsu_key_hex += hex_byte;
            }
            db<Protocol>(INF) << "[Protocol] MAC Verify - Testing RSU " << idx << " (" << rsu.address.to_string() << ") key: " << rsu_key_hex << "\n";
            
            MacKeyType calculated_mac = calculate_mac(data, size, rsu.group_key);
            
            // Debug logging: Show calculated MAC for this RSU
            std::string calc_mac_hex = "";
            for (size_t i = 0; i < 16; ++i) {
                char hex_byte[4];
                snprintf(hex_byte, sizeof(hex_byte), "%02X ", calculated_mac[i]);
                calc_mac_hex += hex_byte;
            }
            db<Protocol>(INF) << "[Protocol] MAC Verify - Calculated MAC for RSU " << idx << ": " << calc_mac_hex << "\n";
            
            if (calculated_mac == received_mac) {
                db<Protocol>(TRC) << "[Protocol] MAC verified with RSU " << rsu.address.to_string() << " key\n";
                return true;
            }
        }
        
        db<Protocol>(TRC) << "[Protocol] MAC verification failed - no matching RSU key found\n";
        return false;
    }
    
    // For RSUs: Check MAC against current leader key
    MacKeyType leader_key = LeaderKeyStorage::getInstance().getGroupMacKey();
    
    // Debug logging: Show leader key being used
    std::string leader_key_hex = "";
    for (size_t i = 0; i < 16; ++i) {
        char hex_byte[4];
        snprintf(hex_byte, sizeof(hex_byte), "%02X ", leader_key[i]);
        leader_key_hex += hex_byte;
    }
    db<Protocol>(INF) << "[Protocol] MAC Verify - RSU checking with leader key: " << leader_key_hex << "\n";
    
    MacKeyType calculated_mac = calculate_mac(data, size, leader_key);
    
    // Debug logging: Show calculated MAC
    std::string calc_mac_hex = "";
    for (size_t i = 0; i < 16; ++i) {
        char hex_byte[4];
        snprintf(hex_byte, sizeof(hex_byte), "%02X ", calculated_mac[i]);
        calc_mac_hex += hex_byte;
    }
    db<Protocol>(INF) << "[Protocol] MAC Verify - RSU calculated MAC: " << calc_mac_hex << "\n";
    
    bool is_valid = (calculated_mac == received_mac);
    
    db<Protocol>(TRC) << "[Protocol] MAC verification " << (is_valid ? "successful" : "failed") << " with leader key\n";
    return is_valid;
}

template <typename NIC>
bool Protocol<NIC>::requires_authentication(const void* data, unsigned int size) {
    if (size < 1) return false;
    
    const uint8_t* message_bytes = static_cast<const uint8_t*>(data);
    uint8_t msg_type = message_bytes[0];
    
    return is_authenticated_message_type(static_cast<typename ProtocolMessage::Type>(msg_type));
}

template <typename NIC>
bool Protocol<NIC>::is_authenticated_message_type(typename ProtocolMessage::Type type) {
    return (type == ProtocolMessage::Type::INTEREST || 
            type == ProtocolMessage::Type::RESPONSE);
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