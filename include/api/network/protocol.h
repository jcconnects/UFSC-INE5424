#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <string>
#include <cstring>
#include <chrono>
#include <cstdint>
#include <cstddef> // Ensure this is included for offsetof
#include <stdexcept> // Ensure this is included for std::invalid_argument
#include <cstdio> // For snprintf in debug logging
#include <algorithm> // For std::any_of
#include <mutex> // For std::mutex and std::lock_guard

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
        
        // Method to set NIC transmission radius
        void setRadius(double radius);
        
        // Neighbor RSU management (for RSUs only)
        void add_neighbor_rsu(unsigned int rsu_id, const MacKeyType& key, const Address& address);
        void clear_neighbor_rsus();
        
        // Get Protocol's own address
        Address address() const;

        static void attach(Observer* obs, Address address);
        static void detach(Observer* obs, Address address);

    private:
        void update(typename NIC::Protocol_Number prot, Buffer * buf) override;
        void handle_status_message(const typename ProtocolMessage::Type& msg_type,
                                  const uint8_t* message_data, unsigned int payload_size,
                                  const Coordinates* sender_coords,
                                  const TimestampFields* timestamps,
                                  const typename NIC::Address& sender_mac);
        
        void handle_req_message(const uint8_t* message_data, unsigned int payload_size,
                               const typename NIC::Address& sender_mac);
        
        void handle_resp_message(const uint8_t* message_data, unsigned int payload_size);
        
        void send_req_message_to_leader(const void* failed_message_data, unsigned int failed_message_size,
                                       const MacKeyType& failed_mac);
        
        // MAC authentication methods - Hybrid approach (message + packet fields)
        MacKeyType calculate_mac(const void* message_data, unsigned int message_size,
                                const Header* header, const TimestampFields* timestamps,
                                const Coordinates* coordinates, const MacKeyType& key);
        bool verify_mac(const void* message_data, unsigned int message_size,
                       const Header* header, const TimestampFields* timestamps,
                       const Coordinates* coordinates, const MacKeyType& received_mac);
        bool requires_authentication(const void* data, unsigned int size);
        bool is_authenticated_message_type(typename ProtocolMessage::Type type);

    private:
        // Neighbor RSU information for RSUs (used when handling REQ messages)
        struct NeighborRSUInfo {
            unsigned int rsu_id;
            MacKeyType key;
            Address address;
            
            NeighborRSUInfo(unsigned int id, const MacKeyType& k, const Address& addr) 
                : rsu_id(id), key(k), address(addr) {}
        };
        std::vector<NeighborRSUInfo> _neighbor_rsus; // Only used by RSUs
        std::mutex _neighbor_rsus_mutex;             // Thread safety for neighbor RSUs
        
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

    // Check if message requires authentication and we have the necessary keys
    if (requires_authentication(data, size)) {
        db<Protocol>(TRC) << "[Protocol] Message requires authentication - checking key availability\n";
        
        bool has_auth_keys = false;
        
        if (_entity_type == EntityType::VEHICLE) {
            // For vehicles: check if we have any known RSUs (leaders)
            if (_vehicle_rsu_manager) {
                auto known_rsus = _vehicle_rsu_manager->get_known_rsus();
                has_auth_keys = !known_rsus.empty();
                db<Protocol>(TRC) << "[Protocol] Vehicle has " << known_rsus.size() << " known RSUs for authentication\n";
            } else {
                db<Protocol>(TRC) << "[Protocol] Vehicle has no RSU manager - no authentication possible\n";
            }
        } else {
            // For RSUs: check if we have a valid leader key (not all zeros)
            MacKeyType leader_key = LeaderKeyStorage::getInstance().getGroupMacKey();
            has_auth_keys = std::any_of(leader_key.begin(), leader_key.end(), [](uint8_t b) { return b != 0; });
            db<Protocol>(TRC) << "[Protocol] RSU leader key is " << (has_auth_keys ? "valid" : "empty") << "\n";
        }
        
        if (!has_auth_keys) {
            db<Protocol>(WRN) << "[Protocol] Dropping authenticated message - no authentication keys available\n";
            return 0; // Drop the message
        }
        
        db<Protocol>(INF) << "[Protocol] Authentication keys available - proceeding with message send\n";
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
    
    // TX timestamp will be set by NIC during transmission - not used in MAC calculation
    packet->timestamps()->tx_timestamp = TimestampType::min(); // Will be overwritten by NIC

    // Set sender location and communication radius
    Coordinates coords;
    coords.radius = _nic->radius();
    LocationService::getCurrentCoordinates(coords.x, coords.y);
    std::memcpy(packet->coordinates(), &coords, sizeof(Coordinates));

    // Initialize authentication fields
    packet->authentication()->has_mac = false;
    packet->authentication()->mac.fill(0);

    // Calculate MAC for INTEREST and RESPONSE messages
    if (requires_authentication(data, size)) {
        db<Protocol>(TRC) << "[Protocol] Calculating Hybrid MAC for authenticated message (message + packet fields)\n";
        
        // Get leader key from storage
        MacKeyType leader_key = LeaderKeyStorage::getInstance().getGroupMacKey();
        
        // Debug logging: Show packet fields being authenticated
        db<Protocol>(INF) << "[Protocol] Hybrid MAC Auth - Header: from_port=" << packet->header()->from_port() 
                          << ", to_port=" << packet->header()->to_port() << ", size=" << packet->header()->size() << "\n";
        db<Protocol>(INF) << "[Protocol] Hybrid MAC Auth - Timestamps: sync=" << packet->timestamps()->is_clock_synchronized 
                          << " (tx_timestamp excluded from MAC calculation)\n";
                db<Protocol>(INF) << "[Protocol] Hybrid MAC Auth - Coordinates: x=" << packet->coordinates()->x
                                  << ", y=" << packet->coordinates()->y << ", radius=" << packet->coordinates()->radius << "\n";
        
        // Debug logging: Show message data
        db<Protocol>(INF) << "[Protocol] Hybrid MAC Auth - Message payload size: " << size << " bytes\n";
        const uint8_t* msg_bytes = static_cast<const uint8_t*>(data);
        std::string msg_hex = "";
        for (unsigned int i = 0; i < std::min(size, 32u); ++i) {  // Log first 32 bytes
            char hex_byte[4];
            snprintf(hex_byte, sizeof(hex_byte), "%02X ", msg_bytes[i]);
            msg_hex += hex_byte;
        }
        db<Protocol>(INF) << "[Protocol] Hybrid MAC Auth - Message data (first " << std::min(size, 32u) << " bytes): " << msg_hex << "\n";
        
        // Debug logging: Show key
        std::string key_hex = "";
        for (size_t i = 0; i < 16; ++i) {
            char hex_byte[4];
            snprintf(hex_byte, sizeof(hex_byte), "%02X ", leader_key[i]);
            key_hex += hex_byte;
        }
        db<Protocol>(INF) << "[Protocol] Hybrid MAC Auth - Key: " << key_hex << "\n";
        
        // Calculate MAC over the message payload + packet fields (hybrid approach)
        MacKeyType calculated_mac = calculate_mac(data, size, packet->header(), 
                                                 packet->timestamps(), packet->coordinates(), leader_key);
        
        // Debug logging: Show calculated MAC (note: detailed MAC calculation logs are in calculate_mac method)
        std::string mac_hex = "";
        for (size_t i = 0; i < 16; ++i) {
            char hex_byte[4];
            snprintf(hex_byte, sizeof(hex_byte), "%02X ", calculated_mac[i]);
            mac_hex += hex_byte;
        }
        db<Protocol>(INF) << "[Protocol] Hybrid MAC Auth - Final calculated MAC: " << mac_hex << "\n";
        
        // Set MAC in packet
        packet->authentication()->mac = calculated_mac;
        packet->authentication()->has_mac = true;
        
        db<Protocol>(INF) << "[Protocol] Added Hybrid MAC authentication to outgoing message (packet fields + payload)\n";
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
    double rx_x, rx_y;
    LocationService::getCurrentCoordinates(rx_x, rx_y);
    // Check if packet is within sender's communication range
                double distance = GeoUtils::euclideanDistance(coords->x, coords->y, rx_x, rx_y);
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
        auto msg_type = static_cast<typename ProtocolMessage::Type>(raw_msg_type);
        if (is_authenticated_message_type(msg_type)) {
            
            db<Protocol>(TRC) << "[Protocol] Verifying Hybrid MAC for authenticated message (packet fields + payload)\n";
            
            // Verify MAC authentication (hybrid approach: message + packet fields)
            // Note: TX timestamp is excluded from MAC calculation for architectural cleanliness
            if (!verify_mac(message_data, payload_size, pkt->header(), 
                           pkt->timestamps(), pkt->coordinates(), pkt->authentication()->mac)) {
                db<Protocol>(WRN) << "[Protocol] Hybrid MAC verification failed - packet may be tampered\n";
                
                // For vehicles: Send REQ message to leader RSU instead of just dropping
                if (_entity_type == EntityType::VEHICLE && _vehicle_rsu_manager) {
                    db<Protocol>(INF) << "[Protocol] Sending REQ message to leader RSU for failed authentication\n";
                    send_req_message_to_leader(message_data, payload_size, pkt->authentication()->mac);
                } else {
                    db<Protocol>(INF) << "[Protocol] RSU dropping message with failed MAC\n";
                }
                
                free(buf);
                return;
            }
            
            db<Protocol>(INF) << "[Protocol] Hybrid MAC verification successful - packet integrity confirmed\n";
        }
            
            // STATUS message interception (no authentication required)
            if (raw_msg_type == static_cast<uint8_t>(ProtocolMessage::Type::STATUS)) {
                db<Protocol>(INF) << "[Protocol] Intercepted STATUS message\n";
                auto msg_type = static_cast<typename ProtocolMessage::Type>(raw_msg_type);
                handle_status_message(msg_type, message_data, payload_size, pkt->coordinates(), pkt->timestamps(), buf->data()->src);
                free(buf);
                return;
            }
            
            // REQ message interception (for RSUs only, no authentication required)
            if (raw_msg_type == static_cast<uint8_t>(ProtocolMessage::Type::REQ) && _entity_type == EntityType::RSU) {
                db<Protocol>(INF) << "[Protocol] Intercepted REQ message at RSU\n";
                handle_req_message(message_data, payload_size, buf->data()->src);
                free(buf);
                return;
            }
            
            // RESP message interception (for vehicles only, no authentication required)
            if (raw_msg_type == static_cast<uint8_t>(ProtocolMessage::Type::RESP) && _entity_type == EntityType::VEHICLE) {
                db<Protocol>(INF) << "[Protocol] Intercepted RESP message at vehicle\n";
                handle_resp_message(message_data, payload_size);
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
    double rsu_x, rsu_y, rsu_radius;
    MacKeyType rsu_key;
    std::memcpy(&rsu_x, payload + offset, sizeof(double));
    offset += sizeof(double);
    std::memcpy(&rsu_y, payload + offset, sizeof(double));
    offset += sizeof(double);
    std::memcpy(&rsu_radius, payload + offset, sizeof(double));
    offset += sizeof(double);
    std::memcpy(&rsu_key, payload + offset, sizeof(MacKeyType));
    
    // Create Protocol address for the RSU
    Address rsu_address(sender_mac, status_msg.origin().port());
    
    // Forward to RSU manager - this will be linked at compile time when VehicleRSUManager is fully defined
    if (_vehicle_rsu_manager) {
        _vehicle_rsu_manager->process_rsu_status(rsu_address, rsu_x, rsu_y, rsu_radius, rsu_key);
        db<Protocol>(INF) << "[Protocol] Forwarded RSU info to manager: x=" << rsu_x 
                          << ", y=" << rsu_y << ", radius=" << rsu_radius << "\n";
    }
}

// Implementation for the new free method
template <typename NIC>
void Protocol<NIC>::free(Buffer* buf) {
    if (_nic) {
        _nic->free(buf);
    }
}

// Implementation for setRadius method
template <typename NIC>
void Protocol<NIC>::setRadius(double radius) {
    if (_nic) {
        _nic->setRadius(radius);
        db<Protocol>(INF) << "[Protocol] NIC transmission radius set to " << radius << "m\n";
    }
}

// MAC Authentication Implementation - Hybrid Approach
template <typename NIC>
MacKeyType Protocol<NIC>::calculate_mac(const void* message_data, unsigned int message_size,
                                        const Header* header, const TimestampFields* timestamps,
                                        const Coordinates* coordinates, const MacKeyType& key) {
    MacKeyType result;
    result.fill(0);
    
    // Create a buffer containing all authenticated fields in deterministic order
    std::vector<uint8_t> auth_data;
    unsigned int total_size = 0;
    
    // 1. Add Header fields (from_port, to_port - excluding size to avoid circular dependency)
    unsigned int header_auth_size = sizeof(Port) * 2; // from_port + to_port
    total_size += header_auth_size;
    
    // 2. Add TimestampFields (only sync status, exclude tx_timestamp for architectural cleanliness)
    unsigned int timestamp_auth_size = sizeof(bool); // Only is_clock_synchronized
    total_size += timestamp_auth_size;
    
    // 3. Add Coordinates (full structure for location integrity)
    unsigned int coords_auth_size = sizeof(Coordinates);
    total_size += coords_auth_size;
    
    // 4. Add message payload
    total_size += message_size;
    
    // Allocate buffer for all authenticated data
    auth_data.resize(total_size);
    unsigned int offset = 0;
    
    // Serialize Header fields
    Port from_port = header->from_port();
    Port to_port = header->to_port();
    std::memcpy(auth_data.data() + offset, &from_port, sizeof(Port));
    offset += sizeof(Port);
    std::memcpy(auth_data.data() + offset, &to_port, sizeof(Port));
    offset += sizeof(Port);
    
    // Serialize TimestampFields (only sync status, exclude tx_timestamp)
    bool sync_status = timestamps->is_clock_synchronized;
    std::memcpy(auth_data.data() + offset, &sync_status, sizeof(bool));
    offset += sizeof(bool);
    // Note: tx_timestamp is intentionally excluded from MAC calculation
    
    // Serialize Coordinates
    std::memcpy(auth_data.data() + offset, coordinates, sizeof(Coordinates));
    offset += sizeof(Coordinates);
    
    // Serialize message payload
    std::memcpy(auth_data.data() + offset, message_data, message_size);
    
    // Debug logging: Show what's being authenticated
    db<Protocol>(INF) << "[Protocol] Hybrid MAC - Authenticating " << total_size << " bytes total:\n";
    db<Protocol>(INF) << "[Protocol] Hybrid MAC - Header: from_port=" << from_port 
                      << ", to_port=" << to_port << " (" << header_auth_size << " bytes)\n";
    db<Protocol>(INF) << "[Protocol] Hybrid MAC - Timestamps: sync=" << sync_status 
                      << " (" << timestamp_auth_size << " bytes, tx_timestamp excluded)\n";
    db<Protocol>(INF) << "[Protocol] Hybrid MAC - Coordinates: x=" << coordinates->x 
                      << ", y=" << coordinates->y << ", radius=" << coordinates->radius 
                      << " (" << coords_auth_size << " bytes)\n";
    db<Protocol>(INF) << "[Protocol] Hybrid MAC - Message payload: " << message_size << " bytes\n";
    
    // XOR-based MAC calculation on combined authenticated data
    const uint8_t* combined_data = auth_data.data();
    for (unsigned int i = 0; i < total_size; ++i) {
        result[i % 16] ^= combined_data[i];
    }
    
    // XOR with the key
    for (size_t i = 0; i < 16; ++i) {
        result[i] ^= key[i];
    }
    
    // Debug logging: Show computed MAC
    std::string computed_mac_hex = "";
    for (size_t i = 0; i < 16; ++i) {
        char hex_byte[4];
        snprintf(hex_byte, sizeof(hex_byte), "%02X ", result[i]);
        computed_mac_hex += hex_byte;
    }
    db<Protocol>(INF) << "[Protocol] Hybrid MAC - Computed MAC: " << computed_mac_hex << "\n";
    
    return result;
}

template <typename NIC>
bool Protocol<NIC>::verify_mac(const void* message_data, unsigned int message_size,
                              const Header* header, const TimestampFields* timestamps,
                              const Coordinates* coordinates, const MacKeyType& received_mac) {
    // Debug logging: Show received MAC
    std::string received_mac_hex = "";
    for (size_t i = 0; i < 16; ++i) {
        char hex_byte[4];
        snprintf(hex_byte, sizeof(hex_byte), "%02X ", received_mac[i]);
        received_mac_hex += hex_byte;
    }
    db<Protocol>(INF) << "[Protocol] Hybrid MAC Verify - Received MAC: " << received_mac_hex << "\n";
    
    // Debug logging: Show packet fields being verified
    db<Protocol>(INF) << "[Protocol] Hybrid MAC Verify - Header: from_port=" << header->from_port() 
                      << ", to_port=" << header->to_port() << ", size=" << header->size() << "\n";
    db<Protocol>(INF) << "[Protocol] Hybrid MAC Verify - Timestamps: sync=" << timestamps->is_clock_synchronized 
                      << " (tx_timestamp excluded from MAC calculation)\n";
    db<Protocol>(INF) << "[Protocol] Hybrid MAC Verify - Coordinates: x=" << coordinates->x 
                      << ", y=" << coordinates->y << ", radius=" << coordinates->radius << "\n";
    db<Protocol>(INF) << "[Protocol] Hybrid MAC Verify - Message payload size: " << message_size << " bytes\n";
    
    // Debug logging: Show message data being verified
    const uint8_t* msg_bytes = static_cast<const uint8_t*>(message_data);
    std::string msg_hex = "";
    for (unsigned int i = 0; i < std::min(message_size, 32u); ++i) {  // Log first 32 bytes
        char hex_byte[4];
        snprintf(hex_byte, sizeof(hex_byte), "%02X ", msg_bytes[i]);
        msg_hex += hex_byte;
    }
    db<Protocol>(INF) << "[Protocol] Hybrid MAC Verify - Message data (first " << std::min(message_size, 32u) << " bytes): " << msg_hex << "\n";
    
    // For vehicles: Check MAC against all known RSU keys AND neighbor RSU keys
    if (_entity_type == EntityType::VEHICLE && _vehicle_rsu_manager) {
        auto known_rsus = _vehicle_rsu_manager->get_known_rsus();
        auto neighbor_keys = _vehicle_rsu_manager->get_neighbor_rsu_keys();
        
        db<Protocol>(INF) << "[Protocol] Hybrid MAC Verify - Vehicle checking against " << known_rsus.size() 
                          << " known RSU keys and " << neighbor_keys.size() << " neighbor RSU keys\n";
        
        // First check known RSUs
        for (size_t idx = 0; idx < known_rsus.size(); ++idx) {
            const auto& rsu = known_rsus[idx];
            
            // Debug logging: Show RSU key being tested
            std::string rsu_key_hex = "";
            for (size_t i = 0; i < 16; ++i) {
                char hex_byte[4];
                snprintf(hex_byte, sizeof(hex_byte), "%02X ", rsu.group_key[i]);
                rsu_key_hex += hex_byte;
            }
            db<Protocol>(INF) << "[Protocol] Hybrid MAC Verify - Testing known RSU " << idx << " (" << rsu.address.to_string() << ") key: " << rsu_key_hex << "\n";
            
            MacKeyType calculated_mac = calculate_mac(message_data, message_size, header, timestamps, coordinates, rsu.group_key);
            
            // Debug logging: Show calculated MAC for this key
            std::string calc_mac_hex = "";
            for (size_t i = 0; i < 16; ++i) {
                char hex_byte[4];
                snprintf(hex_byte, sizeof(hex_byte), "%02X ", calculated_mac[i]);
                calc_mac_hex += hex_byte;
            }
            db<Protocol>(INF) << "[Protocol] Hybrid MAC Verify - Known RSU " << idx << " calculated MAC: " << calc_mac_hex << "\n";
            
            if (calculated_mac == received_mac) {
                db<Protocol>(TRC) << "[Protocol] Hybrid MAC verified with known RSU " << rsu.address.to_string() << " key\n";
                return true;
            }
        }
        
        // Then check neighbor RSU keys
        for (size_t idx = 0; idx < neighbor_keys.size(); ++idx) {
            const auto& neighbor_key = neighbor_keys[idx];
            
            // Debug logging: Show neighbor key being tested
            std::string neighbor_key_hex = "";
            for (size_t i = 0; i < 16; ++i) {
                char hex_byte[4];
                snprintf(hex_byte, sizeof(hex_byte), "%02X ", neighbor_key[i]);
                neighbor_key_hex += hex_byte;
            }
            db<Protocol>(INF) << "[Protocol] Hybrid MAC Verify - Testing neighbor RSU " << idx << " key: " << neighbor_key_hex << "\n";
            
            MacKeyType calculated_mac = calculate_mac(message_data, message_size, header, timestamps, coordinates, neighbor_key);
            
            // Debug logging: Show calculated MAC for this neighbor key
            std::string calc_mac_hex = "";
            for (size_t i = 0; i < 16; ++i) {
                char hex_byte[4];
                snprintf(hex_byte, sizeof(hex_byte), "%02X ", calculated_mac[i]);
                calc_mac_hex += hex_byte;
            }
            db<Protocol>(INF) << "[Protocol] Hybrid MAC Verify - Neighbor RSU " << idx << " calculated MAC: " << calc_mac_hex << "\n";
            
            if (calculated_mac == received_mac) {
                db<Protocol>(TRC) << "[Protocol] Hybrid MAC verified with neighbor RSU key " << idx << "\n";
                return true;
            }
        }
        
        db<Protocol>(TRC) << "[Protocol] Hybrid MAC verification failed - no matching RSU or neighbor key found\n";
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
    db<Protocol>(INF) << "[Protocol] Hybrid MAC Verify - RSU checking with leader key: " << leader_key_hex << "\n";
    
    MacKeyType calculated_mac = calculate_mac(message_data, message_size, header, timestamps, coordinates, leader_key);
    
    // Debug logging: Show calculated MAC
    std::string calc_mac_hex = "";
    for (size_t i = 0; i < 16; ++i) {
        char hex_byte[4];
        snprintf(hex_byte, sizeof(hex_byte), "%02X ", calculated_mac[i]);
        calc_mac_hex += hex_byte;
    }
    db<Protocol>(INF) << "[Protocol] Hybrid MAC Verify - RSU calculated MAC: " << calc_mac_hex << "\n";
    
    bool is_valid = (calculated_mac == received_mac);
    
    db<Protocol>(TRC) << "[Protocol] Hybrid MAC verification " << (is_valid ? "successful" : "failed") << " with leader key\n";
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
    //return (type == ProtocolMessage::Type::INTEREST || 
    //        type == ProtocolMessage::Type::RESPONSE);

    return (type == ProtocolMessage::Type::RESPONSE);
}

// REQ message handler implementation
template <typename NIC>
void Protocol<NIC>::handle_req_message(const uint8_t* message_data, unsigned int payload_size,
                                       const typename NIC::Address& sender_mac) {
    db<Protocol>(INF) << "[Protocol] RSU processing REQ message from " 
                      << Ethernet::mac_to_string(sender_mac) << "\n";
    
    // Only RSUs should handle REQ messages
    if (_entity_type != EntityType::RSU) {
        db<Protocol>(WRN) << "[Protocol] Non-RSU entity received REQ message - ignoring\n";
        return;
    }
    
    // Get pointer to RSU instance - we need to access it to search neighbor RSUs
    // For now, we'll use a simple approach: extract the failed MAC from the REQ message
    // and search our neighbor RSUs for a match
    
    // Deserialize the REQ message
    ProtocolMessage req_msg = ProtocolMessage::deserialize(message_data, payload_size);
    if (req_msg.message_type() != ProtocolMessage::Type::REQ) {
        db<Protocol>(WRN) << "[Protocol] Failed to deserialize REQ message\n";
        return;
    }
    
    // REQ message payload contains: original_message_data + failed_mac
    const uint8_t* req_payload = req_msg.value();
    unsigned int req_value_size = req_msg.value_size();
    
    if (req_value_size < sizeof(MacKeyType)) {
        db<Protocol>(WRN) << "[Protocol] REQ message payload too small\n";
        return;
    }
    
    // Extract the failed MAC (last 16 bytes of payload)
    MacKeyType failed_mac;
    unsigned int original_msg_size = req_value_size - sizeof(MacKeyType);
    std::memcpy(&failed_mac, req_payload + original_msg_size, sizeof(MacKeyType));
    
    // Debug logging: Show failed MAC
    std::string failed_mac_hex = "";
    for (size_t i = 0; i < 16; ++i) {
        char hex_byte[4];
        snprintf(hex_byte, sizeof(hex_byte), "%02X ", failed_mac[i]);
        failed_mac_hex += hex_byte;
    }
    db<Protocol>(INF) << "[Protocol] REQ - Searching for MAC: " << failed_mac_hex << "\n";
    
    // Search for matching neighbor RSU key in our Protocol's neighbor list
    // This list is populated by the RSU during initialization
    bool found_match = false;
    MacKeyType matching_key;
    unsigned int matching_rsu_id = 0;
    
    {
        std::lock_guard<std::mutex> lock(_neighbor_rsus_mutex);
        for (const auto& neighbor : _neighbor_rsus) {
            if (neighbor.key == failed_mac) {
                matching_key = neighbor.key;
                matching_rsu_id = neighbor.rsu_id;
                found_match = true;
                db<Protocol>(INF) << "[Protocol] Found matching neighbor RSU " << neighbor.rsu_id << " for failed MAC\n";
                break;
            }
        }
    }
    
    if (found_match) {
        // Send RESP message with the matching key to the requesting vehicle
        Address vehicle_address(sender_mac, req_msg.origin().port());
        
        // Create RESP message with just the neighbor RSU key
        ProtocolMessage* resp_msg = new ProtocolMessage(ProtocolMessage::Type::RESP,
                                                       address(), 0, ProtocolMessage::ZERO,
                                                       &matching_key, sizeof(MacKeyType));
        
        db<Protocol>(INF) << "[Protocol] Sending RESP message to vehicle " 
                          << vehicle_address.to_string() << " with neighbor RSU " << matching_rsu_id << " key\n";
        
        // Send unicast RESP message to the requesting vehicle
        int result = send(address(), vehicle_address, resp_msg->data(), resp_msg->size());
        
        if (result > 0) {
            db<Protocol>(INF) << "[Protocol] Successfully sent RESP message with RSU " << matching_rsu_id << " key\n";
        } else {
            db<Protocol>(WRN) << "[Protocol] Failed to send RESP message for RSU " << matching_rsu_id << "\n";
        }
        
        delete resp_msg;
    } else {
        db<Protocol>(INF) << "[Protocol] No matching neighbor RSU found for failed MAC\n";
    }
}

// RESP message handler implementation
template <typename NIC>
void Protocol<NIC>::handle_resp_message(const uint8_t* message_data, unsigned int payload_size) {
    db<Protocol>(INF) << "[Protocol] Vehicle processing RESP message\n";
    
    // Only vehicles should handle RESP messages
    if (_entity_type != EntityType::VEHICLE || !_vehicle_rsu_manager) {
        db<Protocol>(WRN) << "[Protocol] Non-vehicle entity or no RSU manager for RESP message - ignoring\n";
        return;
    }
    
    // Deserialize the RESP message
    ProtocolMessage resp_msg = ProtocolMessage::deserialize(message_data, payload_size);
    if (resp_msg.message_type() != ProtocolMessage::Type::RESP) {
        db<Protocol>(WRN) << "[Protocol] Failed to deserialize RESP message\n";
        return;
    }
    
    // RESP message payload contains the neighbor RSU key
    const uint8_t* resp_payload = resp_msg.value();
    unsigned int resp_value_size = resp_msg.value_size();
    
    if (resp_value_size != sizeof(MacKeyType)) {
        db<Protocol>(WRN) << "[Protocol] RESP message payload size mismatch: expected " 
                          << sizeof(MacKeyType) << ", got " << resp_value_size << "\n";
        return;
    }
    
    // Extract the neighbor RSU key
    MacKeyType neighbor_key;
    std::memcpy(&neighbor_key, resp_payload, sizeof(MacKeyType));
    
    // Debug logging: Show received neighbor key
    std::string neighbor_key_hex = "";
    for (size_t i = 0; i < 16; ++i) {
        char hex_byte[4];
        snprintf(hex_byte, sizeof(hex_byte), "%02X ", neighbor_key[i]);
        neighbor_key_hex += hex_byte;
    }
    db<Protocol>(INF) << "[Protocol] RESP - Received neighbor RSU key: " << neighbor_key_hex << "\n";
    
    // Add the neighbor RSU key to our collection
    _vehicle_rsu_manager->add_neighbor_rsu_key(neighbor_key);
    
    db<Protocol>(INF) << "[Protocol] Successfully added neighbor RSU key to vehicle storage\n";
}

// REQ message sending implementation
template <typename NIC>
void Protocol<NIC>::send_req_message_to_leader(const void* failed_message_data, unsigned int failed_message_size,
                                              const MacKeyType& failed_mac) {
    db<Protocol>(INF) << "[Protocol] Preparing REQ message for leader RSU\n";
    
    // Only vehicles should send REQ messages
    if (_entity_type != EntityType::VEHICLE || !_vehicle_rsu_manager) {
        db<Protocol>(WRN) << "[Protocol] Non-vehicle entity or no RSU manager - cannot send REQ\n";
        return;
    }
    
    // Get current leader
    auto current_leader = _vehicle_rsu_manager->get_current_leader();
    if (!current_leader) {
        db<Protocol>(WRN) << "[Protocol] No current leader RSU - cannot send REQ message\n";
        return;
    }
    
    db<Protocol>(INF) << "[Protocol] Sending REQ to leader RSU: " << current_leader->address.to_string() << "\n";
    
    // Create REQ message payload: original_message_data + failed_mac
    unsigned int req_payload_size = failed_message_size + sizeof(MacKeyType);
    std::vector<uint8_t> req_payload(req_payload_size);
    
    // Copy original message data
    std::memcpy(req_payload.data(), failed_message_data, failed_message_size);
    // Append failed MAC
    std::memcpy(req_payload.data() + failed_message_size, &failed_mac, sizeof(MacKeyType));
    
    // Create REQ message
    ProtocolMessage* req_msg = new ProtocolMessage(ProtocolMessage::Type::REQ,
                                                  address(), 0, ProtocolMessage::ZERO,
                                                  req_payload.data(), req_payload_size);
    
    // Send unicast REQ message to leader RSU
    int result = send(address(), current_leader->address, req_msg->data(), req_msg->size());
    
    if (result > 0) {
        db<Protocol>(INF) << "[Protocol] Successfully sent REQ message to leader\n";
    } else {
        db<Protocol>(WRN) << "[Protocol] Failed to send REQ message to leader\n";
    }
    
    delete req_msg;
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

// Neighbor RSU management implementation
template <typename NIC>
void Protocol<NIC>::add_neighbor_rsu(unsigned int rsu_id, const MacKeyType& key, const Address& address) {
    if (_entity_type != EntityType::RSU) {
        db<Protocol>(WRN) << "[Protocol] Attempted to add neighbor RSU to non-RSU entity\n";
        return;
    }
    
    std::lock_guard<std::mutex> lock(_neighbor_rsus_mutex);
    
    // Check if already exists
    for (const auto& neighbor : _neighbor_rsus) {
        if (neighbor.rsu_id == rsu_id) {
            db<Protocol>(INF) << "[Protocol] Neighbor RSU " << rsu_id << " already known\n";
            return;
        }
    }
    
    _neighbor_rsus.emplace_back(rsu_id, key, address);
    db<Protocol>(INF) << "[Protocol] Added neighbor RSU " << rsu_id << " to protocol (total: " << _neighbor_rsus.size() << ")\n";
}

template <typename NIC>
void Protocol<NIC>::clear_neighbor_rsus() {
    if (_entity_type != EntityType::RSU) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(_neighbor_rsus_mutex);
    _neighbor_rsus.clear();
    db<Protocol>(INF) << "[Protocol] Cleared all neighbor RSUs from protocol\n";
}

template <typename NIC>
typename Protocol<NIC>::Address Protocol<NIC>::address() const {
    if (!_nic) {
        return Address(); // Return null address if no NIC
    }
    
    // Use NIC's physical address and a default port based on entity type
    Port protocol_port = (_entity_type == EntityType::RSU) ? 9999 : 8888;
    return Address(_nic->address(), protocol_port);
}

#endif // PROTOCOL_H