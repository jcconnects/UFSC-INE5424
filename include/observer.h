#ifndef OBSERVER_H
#define OBSERVER_H

#include <unistd.h>  // for getpid()
#include "semaphore_wrapper.h"
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
        ~Conditional_Data_Observer() = default;

        virtual void update(Condition c, Observed_Data* d);
        T* updated();

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
    if (c == _rank) {
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
        Concurrent_Observer(C rank) : Conditional_Data_Observer<D, C>(rank) {};
        ~Concurrent_Observer() = default;
        
        void update(C c, D* d) override;
        D* updated();
        
    private:
        SemaphoreWrapper _semaphore;
};

/***************** CONCURRENT_OBSERVER IMPLEMENTATION *************************/
template <typename D, typename C>
void Concurrent_Observer<D, C>::update(C c, D* d) {
    if (c == this->_rank && d != nullptr) {
        // Store a copy of the pointer, don't modify ref_count here
        this->_data.insert(d);
        _semaphore.post();
    }
}

template <typename D, typename C>
D* Concurrent_Observer<D, C>::updated() {
    _semaphore.wait();
    return this->_data.remove();
}
/*******************************************************************************/

#endif // OBSERVER_H
