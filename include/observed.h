#ifndef OBSERVED_H
#define OBSERVED_H

#include <pthread.h>
#include <iostream>
#include <functional>
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
        bool notifyAll(T* d);
        
        // Specialized method for internal broadcast that handles buffer cloning
        // Takes a buffer cloning function and source port to skip
        bool notifyInternalBroadcast(
            T* original_buf, 
            Condition broadcast_port,
            Condition source_port,
            std::function<T*(T*)> clone_buffer
        );
        
        // Add method to get observers list for broadcast cloning
        Observers& get_observers() { return _observers; }

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

// This method is called by Protocol to notify observers
template <typename T, typename C>
bool Conditionally_Data_Observed<T, C>::notify(C c, T* d) {
    bool notified = false;

    // Note: For INTERNAL_BROADCAST_PORT (Port 1), use notifyInternalBroadcast 
    // which handles buffer cloning and distribution

    for (typename Observers::Iterator obs = _observers.begin(); obs != _observers.end(); ++obs) {
        if ((*obs)->rank() == c) {
            (*obs)->update(c, d);
            notified = true;
        }
    }

    return notified;
}

template <typename T, typename C>
bool Conditionally_Data_Observed<T, C>::notifyAll(T* d) {
    bool notified = false;

    for (typename Observers::Iterator obs = _observers.begin(); obs != _observers.end(); ++obs) {
        (*obs)->update((*obs)->rank(), d);
        notified = true;
    }

    return notified;
}

// Specialized method for internal broadcast with buffer cloning
template <typename T, typename C>
bool Conditionally_Data_Observed<T, C>::notifyInternalBroadcast(
    T* original_buf,
    C broadcast_port,
    C source_port,
    std::function<T*(T*)> clone_buffer
) {
    bool any_notified = false;
    
    // Iterate through all observers and notify them with cloned buffers as needed
    for (typename Observers::Iterator it = _observers.begin(); it != _observers.end(); ++it) {
        // Skip sending to the originator of the message (prevent feedback loops)
        if ((*it)->rank() == source_port) {
            continue;
        }
        
        T* observer_buf = nullptr;
        
        // First observer can use the original buffer (optimization)
        if (!any_notified) {
            observer_buf = original_buf;
        } else {
            // Clone the buffer using the provided function
            observer_buf = clone_buffer(original_buf);
            if (!observer_buf) {
                continue; // Skip this observer if buffer allocation failed
            }
        }
        
        // Notify observer with the internal broadcast condition
        (*it)->update(broadcast_port, observer_buf);
        any_notified = true;
    }
    
    return any_notified;
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
        
        // Add specialized notifyInternalBroadcast that handles thread safety
        bool notifyInternalBroadcast(
            D* original_buf, 
            C broadcast_port,
            C source_port,
            std::function<D*(D*)> clone_buffer
        );
        
    private:
        pthread_mutex_t _mtx;
        Observers _observers;
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
    
    // Check for internal broadcast condition (Port 1)
    if (c == 1 && std::is_integral<C>::value) {
        // Error: For Port 1, use notifyInternalBroadcast instead
        // This method doesn't handle buffer cloning
        pthread_mutex_unlock(&_mtx);
        return false;
    } else {
        // Normal notification for all other ports (including Port 0): 
        // Notify only matching observers
        for (typename Observers::Iterator obs = _observers.begin(); obs != _observers.end(); ++obs) {
            if ((*obs)->rank() == c) {
                (*obs)->update(c, d); // Notify with original condition
                notified = true;
                // Optimization: If only one observer per condition is expected,
                // you could potentially break here.
                break; // Assuming only one observer per port
            }
        }
    }
    
    pthread_mutex_unlock(&_mtx);
    return notified;
}

// Thread-safe implementation of notifyInternalBroadcast
template <typename T, typename C>
bool Concurrent_Observed<T, C>::notifyInternalBroadcast(
    T* original_buf, 
    C broadcast_port,
    C source_port,
    std::function<T*(T*)> clone_buffer
) {
    pthread_mutex_lock(&_mtx);
    
    bool result = Conditionally_Data_Observed<T, C>::notifyInternalBroadcast(
        original_buf, broadcast_port, source_port, clone_buffer);
    
    pthread_mutex_unlock(&_mtx);
    return result;
}

/*********************************************************************************************/

#endif // OBSERVED_H
