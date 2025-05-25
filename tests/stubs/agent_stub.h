#include "api/framework/bus.h"
#include "api/framework/gateway.h"
#include "api/util/debug.h"

class AgentStub {
    public:
        typedef CAN::Observer Observer;
        typedef CAN::Message Message;
        typedef CAN::Address Address;

        AgentStub(CAN* can, Condition::Type type, Message::Unit unit);
        AgentStub(CAN* can, Message::Unit interest, Message::Unit producer);
        ~AgentStub();

        int send(Message::Microseconds period);
        int receive(Message* msg);

    private:
        CAN* _can;
        Observer* _interest_observer;
        Observer* _producer_observer;
        Message::Unit _interest_unit;
        Message::Unit _producer_unit;
};

AgentStub::AgentStub(CAN* can, Message::Unit interest, Message::Unit producer) : _can(can), _interest_unit(interest), _producer_unit(producer) 
{
    Condition interest_condition(interest, Message::Type::RESPONSE);
    Condition producer_condition(producer, Message::Type::INTEREST);
    
    _interest_observer = new Observer(interest_condition);
    _producer_observer = new Observer(producer_condition);

    _can->attach(_interest_observer, interest_condition);
    _can->attach(_producer_observer, producer_condition);
}

AgentStub::AgentStub(CAN* can, Condition::Type type, Message::Unit unit) : _can(can), _interest_unit(unit), _producer_unit(unit) 
{
    Condition condition(unit, type);
    _interest_observer = new Observer(condition);
    _producer_observer = new Observer(condition);

    _can->attach(_interest_observer, condition);
    _can->attach(_producer_observer, condition);
}	

AgentStub::~AgentStub() {
    Condition interest_condition(_interest_unit, Message::Type::RESPONSE);
    Condition producer_condition(_producer_unit, Message::Type::INTEREST);

    _can->detach(_interest_observer, interest_condition);
    _can->detach(_producer_observer, producer_condition);
}

int AgentStub::send(Message::Microseconds period) {
    // create a message with the types in condition
    Message msg(Message::Type::INTEREST, Address({}, 1), _producer_unit, period);
    return _can->send(&msg);
}

int AgentStub::receive(Message* msg) {
    *msg = *_producer_observer->updated();

    return msg->size();
}