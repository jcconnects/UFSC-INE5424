#ifndef COMPONENT_TYPE_H
#define COMPONENT_TYPE_H

// Define ComponentType enum
enum class ComponentType : std::uint8_t {
    UNKNOWN = 0,
    GATEWAY,
    PRODUCER,
    CONSUMER,
    PRODUCER_CONSUMER // Dual role component
};


#endif // COMPONENT_TYPE_H 