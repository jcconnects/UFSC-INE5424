#ifndef COMPONENT_TYPE_H
#define COMPONENT_TYPE_H

#include <cstdint> // Add this for std::uint8_t

// Define ComponentType enum
enum class ComponentType : std::uint8_t {
    UNKNOWN = 0,
    GATEWAY,
    PRODUCER,
    CONSUMER,
    PRODUCER_CONSUMER // Dual role component
};


#endif // COMPONENT_TYPE_H 