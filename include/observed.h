#ifndef OBSERVED_H
#define OBSERVED_H

#include <pthread.h>
#include <iostream>
#include "list.h"

// Forward declarations for Conditional Observer class
template <typename T, typename Condition>
class Conditional_Data_Observer;

// Conditionally Observed Class
template <typename T, typename Condition>
class Conditionally_Data_Observed {
    public:
        typedef T Observed_Data;
        typedef Condition Observing_Condition;
        typedef Conditional_Data_Observer<T, Condition> Observer;
        typedef Ordered_List<Conditional_Data_Observer<T, Condition>, Condition> Observers;

    public:
        Conditionally_Data_Observed() = default;
        virtual ~Conditionally_Data_Observed() = default;

        void attach(Observer* o, Condition c);
        void detach(Observer* o, Condition c);
        bool notify(Condition c, T* d);

    protected:
        Observers _observers;
};

/****************** CONDITIONALLY_DATA_OBSERVED IMPLEMENTATION *************/
template <typename T, typename C>
void Conditionally_Data_Observed<T, C>::attach(Observer* o, C c) {
    _observers.insert(o);
}

template <typename T, typename C>
void Conditionally_Data_Observed<T, C>::detach(Observer* o, C c) {
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
/****************************************************************************/


// Foward declaration for Concurrent_Observer Class
template <typename D, typename C = void>
class Concurrent_Observer;

// Concurrent Observed
template<typename D, typename C>
class Concurrent_Observed : public Conditionally_Data_Observed<D, C>{
    friend class Concurrent_Observer<D, C>;
    
    public:
        typedef D Observed_Data;
        typedef C Observing_Condition;
        typedef Ordered_List<Concurrent_Observer<D, C>, C> Observers;
    
    public:
        Concurrent_Observed();
        ~Concurrent_Observed();
        
        void attach(Concurrent_Observer<D, C>* o, C c) ;
        void detach(Concurrent_Observer<D, C>* o, C c) ;
        bool notify(C c, D* d);
        
    private:
        pthread_mutex_t _mtx;
};

/************************* CONCURRENT_OBSERVED IMPLEMENTATION *****************************/
template <typename T, typename C>
Concurrent_Observed<T, C>::Concurrent_Observed() {
    pthread_mutex_init(&_mtx, nullptr);
}

template <typename T, typename C>
Concurrent_Observed<T, C>::~Concurrent_Observed() {
    pthread_mutex_destroy(&_mtx);
}

template <typename T, typename C>
void Concurrent_Observed<T, C>::attach(Concurrent_Observer<T, C>* o, C c) {
    pthread_mutex_lock(&_mtx);
    this->_observers.insert(o);
    pthread_mutex_unlock(&_mtx);
}

template <typename T, typename C>
void Concurrent_Observed<T, C>::detach(Concurrent_Observer<T, C>* o, C c) {
    pthread_mutex_lock(&_mtx);
    this->_observers.remove(o);
    pthread_mutex_unlock(&_mtx);
}

template <typename T, typename C>
bool Concurrent_Observed<T, C>::notify(C c, T* d) {
    pthread_mutex_lock(&_mtx);
    bool notified = false;
    
    for (typename Observers::Iterator obs = this->_observers.begin(); obs != this->_observers.end(); ++obs) {
        if ((*obs)->rank() == c) {
            (*obs)->update(c, d);
            notified = true;
        }
    }
    
    pthread_mutex_unlock(&_mtx);
    return notified;
}

/*********************************************************************************************/

#endif // OBSERVED_H
