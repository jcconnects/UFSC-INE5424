#ifndef ECU_COMPONENT_H
#define ECU_COMPONENT_H

#include <string>

#include "../api/util/debug.h"
#include "api/framework/agent.h"
#include "api/network/bus.h"
#include "app/vehicle.h"
#include "app/datatypes.h"

class ECUComponent : public Agent {
    public:
        // ECU receives a port for identification
        ECUComponent(CAN* can, const Message::Origin addr, const std::string& name = "ECUComponent");
        ~ECUComponent() = default;

        void handle_response(Message* msg) override;

        Value get(Unit type) override { return Value(); } 
};

/*********** ECU Component Implementation **********/
inline ECUComponent::ECUComponent(CAN* can, const Message::Origin addr, const std::string& name) : Agent(can, name, static_cast<std::uint32_t>(DataTypes::EXTERNAL_POINT_CLOUD_XYZ), CAN::Message::Type::UNKNOWN, addr) {}

inline void ECUComponent::handle_response(Message* msg) {
    db<ECUComponent>(INF) << "[ECUComponent] " << name() 
                         << " received RESPONSE message from " << msg->origin().to_string()
                         << " for unit " << msg->unit()
                         << " with " << msg->value_size() << " bytes of data\n";
}

#endif // ECU_COMPONENT_H 