#ifndef INITIALIZER_H
#define INITIALIZER_H

#include <string>

#include "communicator.h"
#include "nic.h"
#include "protocol.h"
#include "socketEngine.h"
#include "vehicle.h"
#include "debug.h"

// Initializer class responsible for creating and managing a single vehicle process
class Initializer {
    public:
        typedef NIC<SocketEngine> VehicleNIC;
        typedef Protocol<VehicleNIC> CProtocol;

        Initializer() = default;

        ~Initializer() = default;

        // Start the vehicle process
        static Vehicle* create_vehicle(unsigned int id);
};

/********** Initializer Implementation ***********/
Vehicle* Initializer::create_vehicle(unsigned int id) {
    VehicleNIC* nic = new VehicleNIC();
    CProtocol* protocol = new CProtocol(nic);
    return new Vehicle(id, nic, protocol);
}

#endif // INITIALIZER_H
