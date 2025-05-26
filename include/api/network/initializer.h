#ifndef INITIALIZER_H
#define INITIALIZER_H

#include "api/network/socketEngine.h"
#include "api/network/nic.h"
#include "api/network/protocol.h"
#include "api/network/communicator.h"
#include "api/network/message.h"
#include "api/network/bus.h"


/**
 * @brief This class initializes the API
 *
 * 
 * This class is only responsible for starting the API.
 * It allows you to create a network interface (NIC) and a protocol,
 * which will be used in the system. The idea is that it is independent
 * of the application in which the developer will use the API.
 */
class Initializer {

    public:
        typedef NIC<SocketEngine> NIC_T;
        typedef Protocol<NIC_T> Protocol_T;
        typedef Communicator<Protocol_T> Communicator_T;
        typedef Communicator_T::Message_T Message;

        Initializer() = default;
        ~Initializer() = default;

        /**
         * @brief Creates a network interface abstraction.
         * 
         * @return The created NIC
         */
        static NIC_T* create_nic();

        /**
         * @brief Creates a protocol for system communication
         * @param nic a previously created network interface
         * 
         * @return The created Protocol, associated with the network interface passed as a parameter
         */
        static Protocol_T* create_protocol(NIC_T* nic);
};

/********** Initializer Implementation ***********/

Initializer::NIC_T* Initializer::create_nic() {
    return new NIC_T();
}

Initializer::Protocol_T* Initializer::create_protocol(NIC_T* nic) {
    if (!nic) {
        throw std::invalid_argument("NIC cannot be null");
    }

    return new Protocol_T(nic);
}

#endif // INITIALIZER_H
