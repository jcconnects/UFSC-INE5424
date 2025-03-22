#ifndef OBSERVER_H
#define OBSERVER_H

#include <list>
#include <mutex>
#include <condition_variable>

// Forward declarations for template classes
template <typename T>
class List;

template <typename T, typename C>
class Ordered_List;

// Semaphore implementation for Concurrent Observer
class Semaphore {
public:
    Semaphore(int count = 0);

    void p();
    void v();

private:
    int _count;
    std::mutex _mutex;
    std::condition_variable _cv;
};

// Simple List implementation
template<typename T>
class List {
public:
    void insert(T* item) {
        std::lock_guard<std::mutex> lock(_mutex);
        _items.push_back(item);
    }
    
    T* remove() {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_items.empty())
            return nullptr;
        
        T* item = _items.front();
        _items.pop_front();
        return item;
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _items.empty();
    }
private:
    mutable std::mutex _mutex;
    std::list<T*> _items;
};

// Simple Ordered List implementation
template<typename T, typename C>
class Ordered_List {
public:
    class Iterator {
    public:
        Iterator(typename std::list<T*>::iterator it);
        
        T* operator*();
        T* operator->();
        
        Iterator& operator++();
        
        bool operator!=(const Iterator& other);
        
    private:
        typename std::list<T*>::iterator _it;
    };

    void insert(T* item); 
    void remove(T* item);

    Iterator begin();
    Iterator end();

private:
    mutable std::mutex _mutex;
    std::list<T*> _items;
};

// Fundamentals for Observer X Observed
template <typename T, typename Condition>
class Conditional_Data_Observer {
    // friend class Conditionally_Data_Observed<T, Condition>;
public:
    typedef T Observed_Data;
    typedef Condition Observing_Condition;

    Conditional_Data_Observer() = default;
    virtual ~Conditional_Data_Observer() = default;

    virtual void update(Condition c, T* d) = 0;
    virtual Condition rank() const = 0;
};

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

// Conditional Observer x Conditionally Observed with Data decoupled by a Semaphore
template<typename D, typename C = void>
class Concurrent_Observer;  // Forward declaration

template<typename D, typename C>
class Concurrent_Observed
{
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

template<typename D, typename C>
class Concurrent_Observer
{
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
    Semaphore _semaphore;
    List<D> _data;
    C _rank;
};

// Include template implementations
#include "observer_impl.h"

#endif // OBSERVER_H
