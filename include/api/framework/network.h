#ifndef NETWORK_H
#define NETWORK_H

#include "api/network/initializer.h"
#include "api/network/bus.h"
#include "api/framework/vehicleRSUManager.h"

class Network {
    public:
        enum class EntityType { VEHICLE, RSU };
        typedef Initializer::NIC_T NIC;
        typedef Initializer::Protocol_T Protocol;
        typedef Initializer::Message Message;
        typedef Initializer::Communicator_T Communicator;

        Network(const unsigned int id = 0, EntityType entity_type = EntityType::VEHICLE);
        ~Network();

        void stop() { 
            if (_nic)
                _nic->stop();
        }

        Protocol* channel();
        CAN* bus();
        const NIC::Address address();
        
        // New: Set RSU manager for vehicles
        void set_vehicle_rsu_manager(VehicleRSUManager<Protocol>* manager);
    private:
        unsigned int _id;
        Protocol* _protocol;
        NIC* _nic;
        CAN* _can;
        EntityType _entity_type;
};

inline Network::Network(const unsigned int id, EntityType entity_type) : _id(id), _entity_type(entity_type) {
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

    // Pass entity type to Protocol
    Protocol::EntityType prot_entity_type = (entity_type == EntityType::VEHICLE) ? 
        Protocol::EntityType::VEHICLE : Protocol::EntityType::RSU;
    _protocol = Initializer::create_protocol(_nic, prot_entity_type);
    _can = new CAN();
}

inline Network::~Network() {
    delete _can;
    _can = nullptr;
    delete _protocol;
    _protocol = nullptr;
    delete _nic;
    _nic = nullptr;
}

inline Network::Protocol* Network::channel() {
    return _protocol;
}

inline CAN* Network::bus() {
    return _can;
}

inline const Network::NIC::Address Network::address() {
    return _nic->address();
}

inline void Network::set_vehicle_rsu_manager(VehicleRSUManager<Protocol>* manager) {
    if (_entity_type == EntityType::VEHICLE) {
        _protocol->set_vehicle_rsu_manager(manager);
    }
}

#endif // NETWORK_H