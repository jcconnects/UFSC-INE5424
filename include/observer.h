#ifndef OBSERVER_H
#define OBSERVER_H

#include "semaphore_wrapper.h"
#include "list.h"
#include <atomic>
#include <stdexcept>
#include <unistd.h>  // for getpid()
#include <fcntl.h>   // for O_CREAT, O_EXCL

// Forward declarations for observed classes
template <typename T, typename Condition>
class Conditionally_Data_Observed;

template <typename D, typename C>
class Concurrent_Observed;

// Fundamentals for Observer X Observed
template <typename T, typename Condition>
class Conditional_Data_Observer {
    friend class Conditionally_Data_Observed<T, Condition>;
public:
    typedef T Observed_Data;
    typedef Condition Observing_Condition;

    Conditional_Data_Observer() = default;
    virtual ~Conditional_Data_Observer() = default;

    virtual void update(Condition c, T* d) = 0;
    virtual Condition rank();
protected:
    Condition _rank;
};

// Conditional Observer x Conditionally Observed with Data decoupled by a Semaphore
template<typename D, typename C>
class Concurrent_Observer {
    friend class Concurrent_Observed<D, C>;
public:
    typedef D Observed_Data;
    typedef C Observing_Condition;
public:
    Concurrent_Observer(C rank);
    virtual ~Concurrent_Observer() = default;
    
    void update(C c, D* d);
    D* updated();
    C rank();
    
private:
    SemaphoreWrapper _semaphore;
    List<D> _data;
    C _rank;
};

// Template implementations for Conditional_Data_Observer
template <typename T, typename Condition>
Condition Conditional_Data_Observer<T, Condition>::rank() {
    return _rank;
}

// Template implementations for Concurrent_Observer
template <typename D, typename C>
Concurrent_Observer<D, C>::Concurrent_Observer(C rank)
    : _semaphore(), _rank(rank) {}

template <typename D, typename C>
void Concurrent_Observer<D, C>::update(C c, D* d) {
    if (c == _rank && d != nullptr) {
        // Store a copy of the pointer, don't modify ref_count here
        _data.insert(d);
        _semaphore.post();
    }
}

template <typename D, typename C>
D* Concurrent_Observer<D, C>::updated() {
    _semaphore.wait();
    return _data.remove();
}

template <typename D, typename C>
C Concurrent_Observer<D, C>::rank() {
    return _rank;
}

#endif // OBSERVER_H
