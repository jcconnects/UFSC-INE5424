#ifndef CAN_H
#define CAN_H

#include "api/util/observed.h"
#include "api/traits.h"
#include "api/util/debug.h"
#include "api/network/initializer.h"
#include <cstring>

class Condition {
    public:
        typedef Initializer::Message Message;
        typedef Message::Unit Unit;
        typedef Message::Type Type;

        Condition(Unit unit, Type type);
        Condition() = default;
        ~Condition() = default;

        const Unit unit() const;
        const Type type() const;

        inline bool operator==(const Condition& other) const;
        inline bool operator!=(const Condition& other) const;

    private:
        Unit _unit;
        Type _type;
};

Condition::Condition(Unit unit, Type type) {
    _unit = unit;
    _type = type;
}

const Condition::Unit Condition::unit() const {
    return _unit;
}

const Condition::Type Condition::type() const {
    return _type;
}

bool Condition::operator==(const Condition& other) const {
    return ((_unit == other.unit()) && (_type == other.type()));
}

bool Condition::operator!=(const Condition& other) const {
    return !(*this == other);
}

class CAN : public Concurrent_Observed<Initializer::Message, Condition>{
    public:
        typedef Initializer::Message Message;
        typedef Initializer::Protocol_T::Address Address;
        typedef Message::Unit Unit;
        typedef Message::Type Type;
        typedef Concurrent_Observer<Message, Condition> Observer;
        typedef Concurrent_Observed<Message, Condition> Observed;
        
        CAN() = default;
        ~CAN() = default;

        int send(Message* msg);
        bool notify(Message* buf, Condition c) override;
};

int CAN::send(Message* msg) {
    db<CAN>(TRC) << "CAN::send() called!\n";
    Condition c(msg->unit(), msg->message_type());
    if (!notify(msg, c))
        return 0;
    
    return msg->size();
}

bool CAN::notify(Message* buf, Condition c) {
    pthread_mutex_lock(&_mtx);
    bool notified = false;

    db<CAN>(INF) << "Notifying observers...\n";
    
    for (typename Observers::Iterator obs = _observers.begin(); obs != _observers.end(); ++obs) {

        if ((*obs)->rank() == c || (*obs)->rank().type() == Condition::Type::UNKNOWN) {
            Message* msg = new Message(*buf);
            (*obs)->update((*obs)->rank(), msg);
            notified = true;
        }
    }
    
    pthread_mutex_unlock(&_mtx);
    return notified;
}

#endif // CAN_H