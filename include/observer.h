#ifndef OBSERVER_H
#define OBSERVER_H

#include <unistd.h>  // for getpid()
#include <semaphore.h>
#include <stdexcept>
#include "list.h"

// Forward declarations for Conditionally_Data_Observed class
template <typename T, typename Condition>
class Conditionally_Data_Observed;

// Fundamentals for Observer X Observed
template <typename T, typename Condition>
class Conditional_Data_Observer {
    friend class Conditionally_Data_Observed<T, Condition>;
    
    public:
        typedef T Observed_Data;
        typedef Condition Observing_Condition;
        typedef Conditionally_Data_Observed<T, Condition> Observed;

        Conditional_Data_Observer(Condition c);
        virtual ~Conditional_Data_Observer() = default;

        virtual void update(Condition c, Observed_Data* d);
        virtual T* updated();

        const Condition rank() { return _rank; };
    
    protected:
        Condition _rank;
        List<T> _data;
};

/*********************** CONDITIONAL_DATA_OBSERVER IMPLEMENTATION ************************/
template <typename T, typename Condition>
Conditional_Data_Observer<T, Condition>::Conditional_Data_Observer(Condition c) {
    _rank = c;
}

template <typename T, typename Condition>
void Conditional_Data_Observer<T, Condition>::update(Condition c, Observed_Data* d) {
    // Accept if condition matches rank OR if it's the internal broadcast port (1)
    if (c == _rank || (c == 1 && std::is_integral<Condition>::value)) {
        _data.insert(d);
    }
}

template <typename T, typename Condition>
T* Conditional_Data_Observer<T, Condition>::updated() {
    if (!_data.empty())
        return _data.remove();
    
    return nullptr;
}
/*****************************************************************************************/


// Foward declaration for Concurrent_Observed class;
template <typename D, typename C>
class Concurrent_Observed;

// Conditional Observer x Conditionally Observed with Data decoupled by a Semaphore
template<typename D, typename C>
class Concurrent_Observer : public Conditional_Data_Observer<D, C> {
    friend class Concurrent_Observed<D, C>;

    public:
        typedef D Observed_Data;
        typedef C Observing_Condition;
        typedef Concurrent_Observed<D, C> Observed;
    
    public:
        Concurrent_Observer(C rank) : Conditional_Data_Observer<D, C>(rank) {
            if (sem_init(&_semaphore, 0, 0) != 0) {
                throw std::runtime_error("Failed to create semaphore");
            }
        };
        
        ~Concurrent_Observer() {
            sem_destroy(&_semaphore);
        };
        
        void update(C c, D* d) override;
        D* updated();
        
    private:
        sem_t _semaphore;
};

/***************** CONCURRENT_OBSERVER IMPLEMENTATION *************************/
template <typename D, typename C>
void Concurrent_Observer<D, C>::update(C c, D* d) {
    // Accept if condition matches rank OR if it's the internal broadcast port (1)
    if (c == this->_rank || 
       (c == 1 && std::is_integral<C>::value)) { // Accept INTERNAL_BROADCAST_PORT=1
        // Special case: if d is nullptr, it's a shutdown signal, still post the semaphore
        // to unblock waiting threads, but don't add to the queue
        if (d != nullptr) {
            this->_data.insert(d);
        }
        // Post semaphore even for nullptr or broadcast to unblock threads
        sem_post(&_semaphore);
    }
}

template <typename D, typename C>
D* Concurrent_Observer<D, C>::updated() {
    sem_wait(&_semaphore);
    // If the queue is empty, it means we were signaled to unblock but with no data
    // This happens during shutdown
    if (this->_data.empty()) {
        return nullptr;
    }
    return this->_data.remove();
}
/*******************************************************************************/

#endif // OBSERVER_H
