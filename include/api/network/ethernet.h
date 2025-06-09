#ifndef ETHERNET_H
#define ETHERNET_H

#include <cstdint>
#include <cstring>
#include <sstream>
#include <iomanip>

class Ethernet {

    public:
        static constexpr unsigned int MTU = 1500; // Ethernet MTU = 1500 Bytes
        static constexpr unsigned int MAC_SIZE = 6; 
    
        // Defining MAC address
        struct Address {
            std::uint8_t bytes[MAC_SIZE];
        };

        static const Ethernet::Address NULL_ADDRESS;
        static const Ethernet::Address BROADCAST;
        
        // Protocol Type
        typedef std::uint16_t Protocol;
        
        static constexpr unsigned int HEADER_SIZE = sizeof(Address)*2 + sizeof(Protocol);

        // Defining Ethernet Frame
        struct Frame {
            Address dst;
            Address src;
            Protocol prot;
            std::uint8_t payload[MTU];
        } __attribute__((packed));

        static std::string mac_to_string(Address addr) {
            std::ostringstream oss;
            for (unsigned int i = 0; i < MAC_SIZE; ++i) {
                if (i != 0) oss << ":";
                oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(addr.bytes[i]);
            }
            return oss.str();
        }

        // Constructor / Destructor
        Ethernet() = default;
        ~Ethernet() = default;

}; // all necessary definitions and formats

constexpr unsigned int Ethernet::MTU;

const Ethernet::Address Ethernet::NULL_ADDRESS = {};
const Ethernet::Address Ethernet::BROADCAST = {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};


inline bool operator==(const Ethernet::Address& a, const Ethernet::Address& b) {
    return std::memcmp(a.bytes, b.bytes, Ethernet::MAC_SIZE) == 0;
}

inline bool operator!=(const Ethernet::Address& a, const Ethernet::Address& b) {
    return !(a == b);
}

#endif // ETHERNET_H