#ifndef OBSERVER_H
#define OBSERVER_H

#include <semaphore.h>
#include "api/util/list.h"

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
        typedef Conditionally_Data_Observed<T, void> Observed;

        Conditional_Data_Observer() = default;
        virtual ~Conditional_Data_Observer() = default;

        virtual void update(Observed_Data* d) { _data.insert(d); }
        virtual T* updated() { return _data.empty() ? nullptr : _data.remove(); }

    protected:
        List<T> _data;
};
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
        Concurrent_Observer(C rank);
        ~Concurrent_Observer();
        
        void update(C c, D* d) override;
        D* updated();
        
    private:
        sem_t _semaphore;
};

/***************** CONCURRENT_OBSERVER IMPLEMENTATION *************************/
template <typename D, typename C>
Concurrent_Observer<D, C>::Concurrent_Observer(C rank) : Conditional_Data_Observer<D, C>(rank) {
    sem_init(&_semaphore, 0, 0);
}

template <typename D, typename C>
Concurrent_Observer<D, C>::~Concurrent_Observer() {
    sem_destroy(&_semaphore);
}

template <typename D, typename C>
void Concurrent_Observer<D, C>::update(C c, D* d) {
    if (c == this->_rank) {
        this->_data.insert(d);
        sem_post(&_semaphore);
    }
}

template <typename D, typename C>
D* Concurrent_Observer<D, C>::updated() {
    sem_wait(&_semaphore);
    return this->_data.remove();
}

// Specification for when Condition = void
template <typename D>
class Concurrent_Observer<D, void> : public Conditional_Data_Observer<D, void> {
    friend class Concurrent_Observed<D, void>;

    public:
        typedef D Observed_Data;
        typedef Concurrent_Observed<D, void> Observed;
    
        Concurrent_Observer() { sem_init(&_semaphore, 0, 0); };
        ~Concurrent_Observer() { sem_destroy(&_semaphore); };

        void update(D* d) {
            this->_data.insert(d);
            sem_post(&_semaphore);
        };

        D* updated() {
            sem_wait(&_semaphore);
            return this->_data.remove();
        }
    
    private:
        sem_t _semaphore;
};
/*******************************************************************************/

#endif // OBSERVER_H
