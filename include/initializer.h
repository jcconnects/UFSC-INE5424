#ifndef INITIALIZER_H
#define INITIALIZER_H

#include "communicator.h"
#include "nic.h"
#include "protocol.h"
#include "socketEngine.h"
#include "vehicle.h"
#include "debug.h"
#include "ethernet.h"
#include <memory> // Add include for std::make_unique and std::move

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
        template <typename SpecificComponentType, typename... Args>
        static SpecificComponentType* create_component(Vehicle* vehicle, const std::string& name, Args&&... args);
};

/********** Initializer Implementation ***********/

Initializer::VehicleNIC* Initializer::create_nic() {
    return new VehicleNIC();
}

Initializer::CProtocol* Initializer::create_protocol(VehicleNIC* nic) {
    return new CProtocol(nic);
}

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

template <typename SpecificComponentType, typename... Args>
SpecificComponentType* Initializer::create_component(Vehicle* vehicle, const std::string& name, Args&&... args) {
    if (!vehicle) {
        db<Initializer>(WRN) << "create_component called with null vehicle.\n";
        return nullptr;
    }

    // Get the protocol pointer (already the concrete TheProtocol* type)
    auto* protocol = vehicle->protocol();
    if (!protocol) {
        db<Initializer>(ERR) << "create_component failed: Vehicle protocol is null.\n";
        // Or throw? Depending on expected guarantees
        return nullptr;
    }

    // Get the next available address for this component
    auto address = vehicle->next_component_address();
    db<Initializer>(INF) << "Creating component '" << name << "' with address " << address << "\n";

    // Create the component using std::make_unique for automatic memory management
    // This directly creates the derived type (e.g., SenderComponent)
    auto component_ptr = std::make_unique<SpecificComponentType>(
        vehicle,
        name,
        protocol,
        address,
        std::forward<Args>(args)... // Pass any extra constructor arguments
    );

    // Get the raw pointer to return (optional, for convenience)
    // The caller MUST NOT delete this pointer, Vehicle owns it now.
    SpecificComponentType* raw_ptr = component_ptr.get();

    // Add the component (as unique_ptr<Component>) to the vehicle.
    // Vehicle::add_component now takes unique_ptr and std::move transfers ownership.
    vehicle->add_component(std::move(component_ptr));

    db<Initializer>(INF) << "Component '" << name << "' added to vehicle.\n";
    return raw_ptr;
}

#endif // INITIALIZER_H
