#include "api/network/initializer.h"
#include "api/network/message.h"
#include "api/app/datatypes.h"
#include "api/util/observed.h"
#include "test_stubs/agent_stub.h"


struct CANCondition {
    DataTypes data_type;
    Message::Type message_type;
}

class CAN: public Concurrent_Observed<Message, CANCondition> {

    friend class Initializer;
    friend class Agent_Stub;

    public:

        typedef Message Observed_Data;
        typedef DataTypes Observing_Condition;
        typedef Concurrent_Observer<Message, CANCondition> Observer;
        // Public Destructor
        ~CAN() = default;

        unsigned int send(Message* msg);

    protected:
        CAN();
};

inline bool operator==(const CANCondition& a, const CANCondition& b) {
    return std::memcmp(&a, &b, sizeof(CANCondition)) == 0;
}

inline bool operator!=(const CANCondition& a, const CANCondition& b) {
    return std::memcmp(&a, &b, sizeof(CANCondition)) != 0;
}

CAN::CAN() : Concurrent_Observed<Message, DataTypes>() {}

unsigned int CAN::send(Message* msg) {
    CANCondition c;
    c.message_type = msg->message_type();
    return notify(c, msg);
}

