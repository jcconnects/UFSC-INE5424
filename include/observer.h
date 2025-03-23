#ifndef OBSERVER_H
#define OBSERVER_H

#include <semaphore.h>
#include "list.h"

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
    virtual Condition rank() const = 0;
};

// Conditional Observer x Conditionally Observed with Data decoupled by a Semaphore
template<typename D, typename C>
class Concurrent_Observer {
    friend class Concurrent_Observed<D, C>;
public:
    typedef D Observed_Data;
    typedef C Observing_Condition;
public:
    Concurrent_Observer(C rank): _semaphore(0), _rank(rank) {}
    virtual ~Concurrent_Observer() = default;
    
    void update(C c, D * d);
    D * updated();
    C rank();
    
private:
    sem_t _semaphore;
    List<D> _data;
    C _rank;
};

// Template implementations for Conditional_Data_Observer
template <typename T, typename Condition>
void Conditional_Data_Observer<T, Condition>::update(Condition c, T* d) {
    // TODO: Implement
}

template <typename T, typename Condition>
Condition Conditional_Data_Observer<T, Condition>::rank() const {
    // TODO: Implement
    return Condition();
}

template <typename T, typename C>
void Concurrent_Observer<T, C>::update(C c, T* d) {
    if (c == _rank && d != nullptr) {
        // Store a copy of the pointer, don't modify ref_count here
        _data.insert(d);
        sem_post(&_semaphore);
    }
}

template <typename T, typename C>
T* Concurrent_Observer<T, C>::updated() {
    sem_wait(&_semaphore);
    return _data.remove();
}

template <typename T, typename C>
C Concurrent_Observer<T, C>::rank() {
    return _rank;
}

#endif // OBSERVER_H
