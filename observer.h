#ifndef OBSERVER_H
#define OBSERVER_H

#include <list>
#include <semaphore.h>
#include <stdexcept>
#include <mutex>

// POSIX Semaphore wrapper
class Semaphore {
public:
    Semaphore(int count = 0) {
        if (sem_init(&_sem, 0, count) != 0) {
            throw std::runtime_error("Failed to initialize semaphore");
        }
    }
    
    ~Semaphore() {
        sem_destroy(&_sem);
    }
    
    void p() {
        sem_wait(&_sem);
    }
    
    void v() {
        sem_post(&_sem);
    }
    
private:
    sem_t _sem;
};

// Thread-safe list implementation
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

// Ordered list with rank-based ordering
template<typename T, typename C>
class Ordered_List {
public:
    class Iterator {
    public:
        Iterator(typename std::list<T*>::iterator it) : _it(it) {}
        
        T* operator*() { return *_it; }
        T* operator->() { return *_it; }
        Iterator& operator++() { ++_it; return *this; }
        bool operator!=(const Iterator& other) { return _it != other._it; }
        
    private:
        typename std::list<T*>::iterator _it;
    };

    void insert(T* item) {
        std::lock_guard<std::mutex> lock(_mutex);
        _items.push_back(item);
    }
    
    void remove(T* item) {
        std::lock_guard<std::mutex> lock(_mutex);
        _items.remove(item);
    }
    
    Iterator begin() {
        return Iterator(_items.begin());
    }
    
    Iterator end() {
        return Iterator(_items.end());
    }

private:
    mutable std::mutex _mutex;
    std::list<T*> _items;
};

// Base Observer classes
template <typename T, typename Condition = void>
class Conditional_Data_Observer {
public:
    typedef T Observed_Data;
    typedef Condition Observing_Condition;

    Conditional_Data_Observer(Condition rank) : _rank(rank) {}
    virtual ~Conditional_Data_Observer() = default;

    virtual void update(Condition c, T* d) = 0;
    virtual Condition rank() const { return _rank; }

protected:
    Condition _rank;
};

template <typename T, typename Condition = void>
class Conditionally_Data_Observed {
public:
    typedef T Observed_Data;
    typedef Condition Observing_Condition;
    typedef Ordered_List<Conditional_Data_Observer<T, Condition>, Condition> Observers;

    void attach(Conditional_Data_Observer<T, Condition>* o, Condition c) {
        _observers.insert(o);
    }
    
    void detach(Conditional_Data_Observer<T, Condition>* o, Condition c) {
        _observers.remove(o);
    }
    
    bool notify(Condition c, T* d) {
        bool notified = false;
        for (typename Observers::Iterator obs = _observers.begin(); 
             obs != _observers.end(); ++obs) {
            if ((*obs)->rank() == c) {
                (*obs)->update(c, d);
                notified = true;
            }
        }
        return notified;
    }

private:
    Observers _observers;
};

// Forward declaration for friend relationship
template<typename D, typename C>
class Concurrent_Observed;

// Concurrent Observer classes
template<typename D, typename C>
class Concurrent_Observer {
    friend class Concurrent_Observed<D, C>;
public:
    typedef D Observed_Data;
    typedef C Observing_Condition;

    Concurrent_Observer(C rank) : _semaphore(0), _rank(rank) {}
    virtual ~Concurrent_Observer() = default;

    void update(C c, D* d) {
        if (c == _rank) {
            _data.insert(d);
            _semaphore.v();
        }
    }

    D* updated() {
        _semaphore.p();
        return _data.remove();
    }

    C rank() const { return _rank; }

private:
    Semaphore _semaphore;
    List<D> _data;
    C _rank;
};

template<typename D, typename C>
class Concurrent_Observed {
    friend class Concurrent_Observer<D, C>;
public:
    typedef D Observed_Data;
    typedef C Observing_Condition;
    typedef Ordered_List<Concurrent_Observer<D, C>, C> Observers;

    void attach(Concurrent_Observer<D, C>* o, C c) {
        _observers.insert(o);
    }

    void detach(Concurrent_Observer<D, C>* o, C c) {
        _observers.remove(o);
    }

    bool notify(C c, D* d) {
        bool notified = false;
        for (typename Observers::Iterator obs = _observers.begin(); 
             obs != _observers.end(); ++obs) {
            if ((*obs)->rank() == c) {
                (*obs)->update(c, d);
                notified = true;
            }
        }
        return notified;
    }

private:
    Observers _observers;
};

#endif // OBSERVER_H
