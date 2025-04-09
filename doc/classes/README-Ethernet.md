# Ethernet Implementation

## Overview

This document describes the implementation of the Ethernet class in the project. The implementation provides the necessary data structures and utilities for handling Ethernet frames, including MAC address management and frame formatting.

## Files and Their Purpose

1. **ethernet.h**:
   - Defines the Ethernet class and related structures
   - Implements MAC address handling and formatting
   - Provides Ethernet frame structure definition
   - Includes constants for Ethernet specifications

## Class Structure

The implementation consists of a single class with nested structures:

1. `Ethernet`: Main class containing Ethernet-related definitions
2. `Address`: Nested structure for MAC addresses
3. `Frame`: Nested structure for Ethernet frames

```
+------------------+
|     Ethernet     |
+------------------+
        <>
        | 
    +---+------------------------+
    |                            |
+------------------+    +------------------+
|     Address      |    |      Frame       |
+------------------+    +------------------+
```

## Ethernet Class Implementation

The Ethernet class provides the foundation for Ethernet frame handling with:

1. **Constants**:
   - `MTU`: Maximum Transmission Unit (1500 bytes)
   - `MAC_SIZE`: Size of MAC address (6 bytes)
   - `HEADER_SIZE`: Size of Ethernet frame header
   - `NULL_ADDRESS`: Special MAC address for null/empty addresses

2. **Data Structures**:
   - `Address`: Structure for MAC addresses
   - `Frame`: Structure for complete Ethernet frames
   - `Protocol`: Type definition for protocol numbers

### MAC Address Structure

```cpp
struct Address {
    std::uint8_t bytes[MAC_SIZE];
};
```

### Ethernet Frame Structure

```cpp
struct Frame {
    Address dst;
    Address src;
    Protocol prot;
    std::uint8_t payload[MTU];
} __attribute__((packed));
```

## Utility Functions

### MAC Address Formatting

The class provides a utility function for converting MAC addresses to human-readable strings:

```cpp
static std::string mac_to_string(Address addr) {
    std::ostringstream oss;
    for (unsigned int i = 0; i < MAC_SIZE; ++i) {
        if (i != 0) oss << ":";
        oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') 
            << static_cast<int>(addr.bytes[i]);
    }
    return oss.str();
}
```

## Comparison Operators

The implementation includes comparison operators for MAC addresses:

```cpp
inline bool operator==(const Ethernet::Address& a, const Ethernet::Address& b) {
    return std::memcmp(a.bytes, b.bytes, Ethernet::MAC_SIZE) == 0;
}

inline bool operator!=(const Ethernet::Address& a, const Ethernet::Address& b) {
    return !(a == b);
}
```

## Implementation Details

1. **Memory Layout**:
   - The `Frame` structure is packed to ensure proper alignment
   - MAC addresses are stored as 6-byte arrays
   - Protocol numbers are stored as unsigned integers

2. **Constants**:
   - `MTU` is set to the standard Ethernet MTU of 1500 bytes
   - `MAC_SIZE` is set to 6 bytes (48 bits)
   - `HEADER_SIZE` is calculated as the sum of destination MAC, source MAC, and protocol fields

3. **Null Address**:
   - A special `NULL_ADDRESS` constant is provided for initialization
   - All bytes are set to zero in the null address

## Alignment with Project Requirements

The implementation aligns with the project specifications:

1. **Ethernet Standard Compliance**:
   - Follows standard Ethernet frame structure
   - Uses correct MTU size
   - Implements proper MAC address format

2. **Memory Efficiency**:
   - Uses packed structures to minimize memory usage
   - Implements efficient comparison operators
   - Provides memory-safe operations

3. **Utility Support**:
   - Includes MAC address formatting
   - Provides comparison operators
   - Supports null address initialization 

## Usage Example

```cpp
Ethernet::Frame frame;
// Set destination MAC
std::memcpy(frame.dst.bytes, destination_mac, Ethernet::MAC_SIZE);
// Set source MAC
std::memcpy(frame.src.bytes, source_mac, Ethernet::MAC_SIZE);
// Set protocol
frame.prot = protocol_number;
// Copy payload
std::memcpy(frame.payload, data, data_size);
```

## Conclusion

The Ethernet class provides a robust and efficient implementation of Ethernet frame handling, including MAC address management and frame formatting. The implementation aligns with the project specifications and provides a foundation for future Ethernet-related functionality.
