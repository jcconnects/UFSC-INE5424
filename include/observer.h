#ifndef OBSERVER_H
#define OBSERVER_H

#include <unistd.h>  // for getpid()
#include "semaphore_wrapper.h"
#include "list.h"

// Forward declarations for Conditionally_Data_Observed class
template <typename T, typename Condition>
class Conditionally_Data_Observed;

// Fundamentals for Observer X Observed
template <typename T, typename Condition = void>
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
    if (c == _rank) {
        _data.insert(d);
    }
}

template <typename T, typename Condition>
T* Conditional_Data_Observer<T, Condition>::updated() {
    return _data.empty() ? nullptr : _data.remove();
}

// Specificaton for when Condition = void
template <typename T>
class Conditional_Data_Observer<T, void> {
    friend class Conditionally_Data_Observed<T, void>;

    public:
        typedef T Observed_Data;
        typedef Conditionally_Data_Observed<T, Condition> Observed;

        Conditional_Data_Observer() = default;
        virtual ~Conditionally_Data_Observed() = default;

        virtual void update(Observed_Data* d) { _data.insert(d); }
        virtual T* updated() { return _data.empty() ? nullptr : _data.remove(); }

    private:
        List<T> _data;
};
/*****************************************************************************************/


// Foward declaration for Concurrent_Observed class;
template <typename D, typename C>
class Concurrent_Observed;

// Conditional Observer x Conditionally Observed with Data decoupled by a Semaphore
template<typename D, typename C = void>
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
    if (c == this->_rank) {
        this->_data.insert(d);
        _semaphore.post();
    }
}

template <typename D, typename C>
D* Concurrent_Observer<D, C>::updated() {
    _semaphore.wait();
    return this->_data.remove();
}

// Specification for when Condition = void
template <typename D>
class Concurrent_Observer<D, void> : public Conditional_Data_Observer<D, void> {
    friend class Concurrent_Observed<D, void>;

    public:
        typedef D Observed_Data;
        typedef Concurrent_Observed<D, void> Observed;
    
        Concurrent_Observer() = default;
        ~Concurrent_Observer() = default;

        void update(D* d) {
            this->_data.insert(d);
            _semaphore.post();
        };

        void updated() {
            _semaphore.wait();
            return this->_data.remove();
        }
};
/*******************************************************************************/

#endif // OBSERVER_H
