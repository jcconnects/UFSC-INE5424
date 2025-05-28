#ifndef NETWORK_H
#define NETWORK_H

#include "api/network/initializer.h"
#include "api/network/bus.h"

class Network {
    public:
        typedef Initializer::NIC_T NIC;
        typedef Initializer::Protocol_T Protocol;
        typedef Initializer::Message Message;
        typedef Initializer::Communicator_T Communicator;

        Network(const unsigned int id = 0);
        ~Network();

        void stop() {
            if (_nic) {
                _nic->stop();
            }
        }

        Protocol* channel();
        CAN* bus();
        const NIC::Address address();
        
    private:
        unsigned int _id;
        Protocol* _protocol;
        NIC* _nic;
        CAN* _can;
};

Network::Network(const unsigned int id) : _id(id) {
    _nic = Initializer::create_nic();
    if (id) {
        NIC::Address addr; // We don't set the address here anymore;
        addr.bytes[0] = 0x02; // the NIC gets its address from the SocketEngine.
        addr.bytes[1] = 0x00;
        addr.bytes[2] = 0x00;
        addr.bytes[3] = 0x00;
        addr.bytes[4] = (id >> 8) & 0xFF;
        addr.bytes[5] = id & 0xFF;
        _nic->setAddress(addr);
    }

    _protocol = Initializer::create_protocol(_nic);
    _can = new CAN();
}

Network::~Network() {
    delete _can;
    _nic->stop();
    delete _protocol;
    delete _nic;
}

Network::Protocol* Network::channel() {
    return _protocol;
}

CAN* Network::bus() {
    return _can;
}

const Network::NIC::Address Network::address() {
    return _nic->address();
}

#endif // NETWORK_H