#ifndef COMPONENT_TYPES_HPP
#define COMPONENT_TYPES_HPP

/**
 * @brief Base class for all component data structures
 * 
 * Following EPOS SmartData design principles, this provides a minimal
 * base class for component data with proper polymorphic destruction.
 * All component-specific data structures should inherit from this base.
 */
struct ComponentData {
    virtual ~ComponentData() = default;
};

#endif // COMPONENT_TYPES_HPP 