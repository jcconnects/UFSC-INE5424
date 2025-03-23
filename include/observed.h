#ifndef OBSERVED_H
#define OBSERVED_H

#include "list.h"

// Forward declarations for observer classes
template <typename T, typename Condition>
class Conditional_Data_Observer;

template <typename D, typename C = void>
class Concurrent_Observer;

// Conditionally Observed Class
template <typename T, typename Condition>
class Conditionally_Data_Observed {
public:
    typedef T Observed_Data;
    typedef Condition Observing_Condition;
    typedef Ordered_List<Conditional_Data_Observer<T, Condition>, Condition> Observers;

    Conditionally_Data_Observed() = default;
    virtual ~Conditionally_Data_Observed() = default;

    void attach(Conditional_Data_Observer<T, Condition>* o, Condition c);
    void detach(Conditional_Data_Observer<T, Condition>* o, Condition c);
    bool notify(Condition c, T* d);

private:
    Observers _observers;
};

// Concurrent Observed
template<typename D, typename C>
class Concurrent_Observed {
    friend class Concurrent_Observer<D, C>;
public:
    typedef D Observed_Data;
    typedef C Observing_Condition;
    typedef Ordered_List<Concurrent_Observer<D, C>, C> Observers;
public:
    Concurrent_Observed() {}
    ~Concurrent_Observed() {}
    
    void attach(Concurrent_Observer<D, C> * o, C c);
    void detach(Concurrent_Observer<D, C> * o, C c);
    bool notify(C c, D * d);
    
private:
    mutable std::mutex _mutex;
    Observers _observers;
};

// Template implementations for Conditionally_Data_Observed
template <typename T, typename C>
void Conditionally_Data_Observed<T, C>::attach(Conditional_Data_Observer<T, C>* o, C c) {
    _observers.insert(o);
}

template <typename T, typename C>
void Conditionally_Data_Observed<T, C>::detach(Conditional_Data_Observer<T, C>* o, C c) {
    _observers.remove(o);
}

template <typename T, typename C>
bool Conditionally_Data_Observed<T, C>::notify(C c, T* d) {
    bool notified = false;
    for (typename Observers::Iterator obs = _observers.begin(); obs != _observers.end(); ++obs) {
        if ((*obs)->rank() == c) {
            (*obs)->update(c, d);
            notified = true;
        }
    }
    return notified;
}

// Template implementations for Concurrent_Observed
template <typename T, typename C>
void Concurrent_Observed<T, C>::attach(Concurrent_Observer<T, C>* o, C c) {
    std::lock_guard<std::mutex> lock(_mutex);
    _observers.insert(o);
}

template <typename T, typename C>
void Concurrent_Observed<T, C>::detach(Concurrent_Observer<T, C>* o, C c) {
    std::lock_guard<std::mutex> lock(_mutex);
    _observers.remove(o);
}

template <typename T, typename C>
bool Concurrent_Observed<T, C>::notify(C c, T* d) {
    std::lock_guard<std::mutex> lock(_mutex);
    bool notified = false;
    int observer_count = 0;

    // First count how many observers will receive this notification
    for (typename Observers::Iterator obs = _observers.begin(); obs != _observers.end(); ++obs) {
        if ((*obs)->rank() == c) {
            observer_count++;
        }
    }

    // Only proceed if there are observers
    if (observer_count > 0 && d) {
        notified = true;
        // Update reference count to match the number of observers that will receive this data
        d->ref_count = observer_count;
    }

    // Notify all observers that match the condition
    for (typename Observers::Iterator obs = _observers.begin(); obs != _observers.end(); ++obs) {
        if ((*obs)->rank() == c) {
            (*obs)->update(c, d);
        }
    }

    return notified;
}

#endif // OBSERVED_H
