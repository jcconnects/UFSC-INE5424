#ifndef OBSERVED_H
#define OBSERVED_H

#include <pthread.h>
#include <iostream>
#include "api/util/list.h"

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
        typedef Ordered_List<Observer, Condition> Observers;

    public:
        Conditionally_Data_Observed() = default;
        virtual ~Conditionally_Data_Observed() = default;

        void attach(Observer* o, Condition c);
        void detach(Observer* o, Condition c);
        bool notify(T* d, Condition c);
        bool notify(T* d);

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
bool Conditionally_Data_Observed<T, C>::notify(T* d, C c) {
    bool notified = false;

    for (typename Observers::Iterator obs = _observers.begin(); obs != _observers.end(); ++obs) {
        if ((*obs)->rank() == c) {
            (*obs)->update(c, d);
            notified = true;
        }
    }

    return notified;
}

template <typename T, typename C>
bool Conditionally_Data_Observed<T, C>::notify(T* d) {
    bool notified = false;

    for (typename Observers::Iterator obs = _observers.begin(); obs != _observers.end(); ++obs) {
        (*obs)->update((*obs)->rank(), d);
        notified = true;
    }

    return notified;
}

// Specification for when Condition = void
template <typename T>
class Conditionally_Data_Observed<T, void> {
    public:
        typedef T Observed_Data;
        typedef Conditional_Data_Observer<T, void> Observer;
        typedef List<Observer> Observers;

        Conditionally_Data_Observed() = default;
        virtual ~Conditionally_Data_Observed() = default;

        void attach(Observer* o) { _observers.insert(o); }
        void detach(Observer* o) { _observers.remove(o); }
        bool notify(T* d) {
            bool notified = false;

            for (typename Observers::Iterator obs = _observers.begin(); obs != _observers.end(); ++obs) {
                (*obs)->update(d);
                notified = true;
            }

            return notified;
        }
    
    protected:
        Observers _observers;
};
/****************************************************************************/


// Foward declaration for Concurrent_Observer Class
template <typename D, typename C>
class Concurrent_Observer;

// Concurrent Observed
template<typename D, typename C>
class Concurrent_Observed : public Conditionally_Data_Observed<D, C>{
    friend class Concurrent_Observer<D, C>;
    
    public:
        typedef D Observed_Data;
        typedef C Observing_Condition;
        typedef Concurrent_Observer<D, C> Observer;
        typedef Ordered_List<Observer, C> Observers;
    
    public:
        Concurrent_Observed();
        ~Concurrent_Observed();
        
        void attach(Observer* o, C c) ;
        void detach(Observer* o, C c) ;
        virtual bool notify(D* d, C c);
        
    protected:
        pthread_mutex_t _mtx;
        Observers _observers;
};

/************************* CONCURRENT_OBSERVED IMPLEMENTATION *****************************/
template <typename D, typename C>
Concurrent_Observed<D, C>::Concurrent_Observed() {
    pthread_mutex_init(&_mtx, nullptr);
}

template <typename D, typename C>
Concurrent_Observed<D, C>::~Concurrent_Observed() {
    pthread_mutex_destroy(&_mtx);
}

template <typename D, typename C>
void Concurrent_Observed<D, C>::attach(Observer* o, C c) {
    pthread_mutex_lock(&_mtx);
    _observers.insert(o);
    pthread_mutex_unlock(&_mtx);
}

template <typename D, typename C>
void Concurrent_Observed<D, C>::detach(Observer* o, C c) {
    pthread_mutex_lock(&_mtx);
    _observers.remove(o);
    pthread_mutex_unlock(&_mtx);
}

template <typename D, typename C>
bool Concurrent_Observed<D, C>::notify(D* d, C c) {
    pthread_mutex_lock(&_mtx);
    bool notified = false;
    
    for (typename Observers::Iterator obs = _observers.begin(); obs != _observers.end(); ++obs) {
        if ((*obs)->rank() == c) {
            (*obs)->update(c, d);
            notified = true;
        }
    }
    
    pthread_mutex_unlock(&_mtx);
    return notified;
}

// Specification for when Condition = void
template <typename D>
class Concurrent_Observed<D, void> : public Conditionally_Data_Observed<D, void>{
    friend class Concurrent_Observer<D, void>;

    public:
        typedef D Observed_Data;
        typedef Concurrent_Observer<D, void> Observer;
        typedef List<Observer> Observers;

        Concurrent_Observed() { pthread_mutex_init(&_mtx, nullptr); }
        ~Concurrent_Observed() { pthread_mutex_destroy(&_mtx); }

        void attach(Observer* o) {
            pthread_mutex_lock(&_mtx);
            _observers.insert(o);
            pthread_mutex_unlock(&_mtx);
        }

        void detach(Observer* o) {
            pthread_mutex_lock(&_mtx);
            _observers.remove(o);
            pthread_mutex_unlock(&_mtx);
        }

        bool notify(D* d) {
            pthread_mutex_lock(&_mtx);
            bool notified = false;
            
            for (typename Observers::Iterator obs = _observers.begin(); obs != _observers.end(); ++obs) {
                (*obs)->update(d);
                notified = true;
            }
            
            pthread_mutex_unlock(&_mtx);
            return notified;
        }
    
    private:
        pthread_mutex_t _mtx;
        Observers _observers;
};

/*********************************************************************************************/

#endif // OBSERVED_H
