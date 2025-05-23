#include "api/framework/bus.h"


class AgentStub {
    public:
        AgentStub(CAN* can);
        ~AgentStub() = default;

        void send(const Message& msg);
        Message* receive();

        static CAN* get_can() {
            return &singleton_can;
        }

        CANCondition get_condition() {
            return _condition;
        }

    private:
        CAN* _can_bus;
        CAN::Observer _observer;
        CANCondition _condition;
        static const CAN singleton_can
}

AgentStub::AgentStub(CAN* can) : _can_bus(can) {
    CANCondition _condition;
    memset(&_condition, 0, sizeof(CANCondition));

    can->attach(&_observer, _condition);
}

void AgentStub::send(void* data) {
    Message msg(ME)
    _can_bus->send(&msg);
}

Message* msg AgentStub::receive() {
    return _observer.updated();
}

CAN* CAN::get_can() {
    return &singleton_can;
}
