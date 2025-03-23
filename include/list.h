#ifndef LIST_H
#define LIST_H

#include <list>
#include <mutex>

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

// Template implementations for Ordered_List
template <typename T, typename C>
Ordered_List<T, C>::Iterator::Iterator(typename std::list<T*>::iterator it) : _it(it) {}

template <typename T, typename C>
T* Ordered_List<T, C>::Iterator::operator*() {
    return *_it;
}

template <typename T, typename C>
T* Ordered_List<T, C>::Iterator::operator->() {
    return *_it;
}

template <typename T, typename C>
typename Ordered_List<T, C>::Iterator& Ordered_List<T, C>::Iterator::operator++() {
    ++_it;
    return *this;
}

template <typename T, typename C>
bool Ordered_List<T, C>::Iterator::operator!=(const Iterator& other) {
    return _it != other._it;
}

template <typename T, typename C>
void Ordered_List<T, C>::insert(T* item) {
    std::lock_guard<std::mutex> lock(_mutex);
    _items.push_back(item);
}

template <typename T, typename C>
void Ordered_List<T, C>::remove(T* item) {
    std::lock_guard<std::mutex> lock(_mutex);
    _items.remove(item);
}

template <typename T, typename C>
typename Ordered_List<T, C>::Iterator Ordered_List<T, C>::begin() {
    return Iterator(_items.begin());
}

template <typename T, typename C>
typename Ordered_List<T, C>::Iterator Ordered_List<T, C>::end() {
    return Iterator(_items.end());
}

#endif // LIST_H
