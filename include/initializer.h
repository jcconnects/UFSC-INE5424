#ifndef INITIALIZER_H
#define INITIALIZER_H

#include "communicator.h"
#include "nic.h"
#include "protocol.h"
#include "socketEngine.h"
#include "vehicle.h"
#include "debug.h"
#include "ethernet.h"

// Initializer class responsible for creating and managing a single vehicle process
class Initializer {
    public:
        typedef NIC<SocketEngine> VehicleNIC;
        typedef Protocol<VehicleNIC> CProtocol;

        Initializer() = default;

        ~Initializer() = default;

        // Start the vehicle process
        static Vehicle* create_vehicle(unsigned int id);
        
        // Template method to create a component with its own communicator
        template <typename ComponentType, typename... Args>
        static ComponentType* create_component(Vehicle* vehicle, const std::string& name, Args&&... args);
};

/********** Initializer Implementation ***********/
Vehicle* Initializer::create_vehicle(unsigned int id) {
    // Setting Vehicle virtual MAC Address
    Ethernet::Address addr;
    addr.bytes[0] = 0x02; // local, unicast
    addr.bytes[1] = 0x00;
    addr.bytes[2] = 0x00;
    addr.bytes[3] = 0x00;
    addr.bytes[4] = (id >> 8) & 0xFF;
    addr.bytes[5] = id & 0xFF;

    VehicleNIC* nic = new VehicleNIC();
    nic->setAddress(addr);
    CProtocol* protocol = new CProtocol(nic);
    return new Vehicle(id, nic, protocol);
}

template <typename ComponentType, typename... Args>
ComponentType* Initializer::create_component(Vehicle* vehicle, const std::string& name, Args&&... args) {
    if (!vehicle) {
        return nullptr;
    }
    
    // Get the protocol from the vehicle
    auto* protocol = vehicle->protocol();
    
    // Get the next available address for this component
    auto address = vehicle->next_component_address();
    
    // Create the component with the protocol and address
    ComponentType* component = new ComponentType(vehicle, name, protocol, address, std::forward<Args>(args)...);
    
    // Add the component to the vehicle
    vehicle->add_component(component);
    
    return component;
}

#endif // INITIALIZER_H
