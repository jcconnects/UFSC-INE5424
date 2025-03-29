#ifndef ETHERNET_H
#define ETHERNET_H

#include <cstdint>
#include <cstring>

class Ethernet {

    public:
        static constexpr std::size_t MTU = 1500; // Ethernet MTU = 1500 Bytes
    
        // Defining MAC address
        class Address {
            public:
                static constexpr std::size_t MAC_SIZE = 6; 

            public:
                Address();
                Address(const std::uint8_t* addr);
                ~Address() = default;

                const std::uint8_t* bytes() const;
                bool operator==(const Address& other) const;
                bool operator!=(const Address& other) const;

            private:
                std::uint8_t _addr[MAC_SIZE];
        };
        
        // Protocol Type
        typedef std::uint16_t Protocol;

        // Defining Ethernet Frame
        struct Frame {
            Address dst;
            Address src;
            unsigned char payload[MTU];
        };

        // Constructor / Destructor
        Ethernet();
        virtual ~Ethernet();

        const Address address() const;

    protected:
        Address _address; // MAC address

}; // all necessary definitions and formats

// Address class implementations
Ethernet::Address::Address() { 
    std::memset(_addr, 0, MAC_SIZE); 
}

Ethernet::Address::Address(const std::uint8_t* addr) {
    std::memcpy(_addr, addr, MAC_SIZE);
}

const std::uint8_t* Ethernet::Address::bytes() const { 
    return _addr; 
}

bool Ethernet::Address::operator==(const Address& other) const {
    return std::memcmp(_addr, other._addr, MAC_SIZE) == 0;
}

bool Ethernet::Address::operator!=(const Address& other) const {
    return !(*this == other);
}

// Ethernet class implementations
Ethernet::Ethernet() = default;

Ethernet::~Ethernet() = default;

const Ethernet::Address Ethernet::address() const { 
    return _address; 
}

#endif // ETHERNET_H