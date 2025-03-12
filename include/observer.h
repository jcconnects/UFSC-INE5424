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

// Fundamentals for Observer X Observed
template <typename T, typename Condition = void>
class Conditional_Data_Observer;

template <typename T, typename Condition = void>
class Conditionally_Data_Observed;

// Semaphore implementation for Concurrent Observer
class Semaphore {
public:
    Semaphore(int count = 0) : count(count) {}

    void p() {
        std::unique_lock<std::mutex> lock(mtx);
        while (count == 0) {
            cv.wait(lock);
        }
        count--;
    }

    void v() {
        std::unique_lock<std::mutex> lock(mtx);
        count++;
        cv.notify_one();
    }

private:
    std::mutex mtx;
    std::condition_variable cv;
    int count;
};

// Simple List implementation
template<typename T>
class List {
public:
    void insert(T* item) {
        _items.push_back(item);
    }

    T* remove() {
        if (_items.empty())
            return nullptr;
        
        T* item = _items.front();
        _items.pop_front();
        return item;
    }

private:
    std::list<T*> _items;
};

// Simple Ordered List implementation
template<typename T, typename C>
class Ordered_List {
public:
    class Iterator {
    public:
        Iterator(typename std::list<T*>::iterator it) : _it(it) {}
        
        T* operator*() { return *_it; }
        T* operator->() { return *_it; }
        
        Iterator& operator++() {
            ++_it;
            return *this;
        }
        
        bool operator!=(const Iterator& other) const {
            return _it != other._it;
        }
        
    private:
        typename std::list<T*>::iterator _it;
    };

    void insert(T* item) {
        _items.push_back(item);
    }

    void remove(T* item) {
        _items.remove(item);
    }

    Iterator begin() {
        return Iterator(_items.begin());
    }

    Iterator end() {
        return Iterator(_items.end());
    }

private:
    std::list<T*> _items;
};

// Conditional Observer x Conditionally Observed with Data decoupled by a Semaphore
template<typename D, typename C = void>
class Concurrent_Observer;

template<typename D, typename C = void>
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
    
    void attach(Concurrent_Observer<D, C> * o, C c) {
        _observers.insert(o);
    }
    
    void detach(Concurrent_Observer<D, C> * o, C c) {
        _observers.remove(o);
    }
    
    bool notify(C c, D * d) {
        bool notified = false;
        for(typename Observers::Iterator obs = _observers.begin(); obs != _observers.end(); ++obs) {
            if((*obs)->rank() == c) {
                (*obs)->update(c, d);
                notified = true;
            }
        }
        return notified;
    }
    
private:
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
    Concurrent_Observer(): _semaphore(0) {}
    ~Concurrent_Observer() {}
    
    void update(C c, D * d) {
        _data.insert(d);
        _semaphore.v();
    }
    
    D * updated() {
        _semaphore.p();
        return _data.remove();
    }
    
    // Added rank method to make the code compile
    C rank() const { return C(); }
    
private:
    Semaphore _semaphore;
    List<D> _data;
};

#endif // OBSERVER_H
