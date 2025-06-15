#ifndef COMPONENT_FUNCTIONS_HPP
#define COMPONENT_FUNCTIONS_HPP

#include "component_types.hpp"
#include <vector>
#include <cstdint>

// Forward declarations to resolve circular dependencies
class Agent;

/**
 * @brief Function pointer type for data production
 * 
 * This function is called when an Agent needs to generate data in response
 * to an INTEREST message. Following EPOS SmartData principles, this allows
 * pure function-based data generation without inheritance.
 * 
 * @param unit The data unit being requested
 * @param data Pointer to component-specific data structure
 * @return Value containing the generated data
 */
typedef std::vector<std::uint8_t> (*DataProducer)(std::uint32_t unit, ComponentData* data);

/**
 * @brief Function pointer type for response handling
 * 
 * This function is called when an Agent receives a RESPONSE message.
 * Following EPOS SmartData principles, this allows pure function-based
 * response processing without inheritance.
 * 
 * @param msg Pointer to the received message
 * @param data Pointer to component-specific data structure
 */
typedef void (*ResponseHandler)(void* msg, ComponentData* data);

#endif // COMPONENT_FUNCTIONS_HPP 