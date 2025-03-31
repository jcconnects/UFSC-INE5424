#ifndef LIST_H
#define LIST_H

#include <list>
#include <mutex>

// Simple List implementation
template<typename T>
class List {
public:
    List() = default;
    ~List() = default;

    void insert(T* item);
    T* remove();
    bool empty() const;

private:
    std::list<T*> _list;
    mutable std::mutex _mutex;
};

// Simple Ordered List implementation
template<typename T, typename R>
class Ordered_List {
public:
    typedef typename std::list<T*>::iterator Iterator;

    Ordered_List() = default;
    ~Ordered_List() = default;

    void insert(T* item);
    void remove(T* item);
    Iterator begin();
    Iterator end();
    bool empty() const;

private:
    std::list<T*> _list;
    mutable std::mutex _mutex;
};

// List implementations
template<typename T>
void List<T>::insert(T* item) {
    std::lock_guard<std::mutex> lock(_mutex);
    _list.push_back(item);
}

template<typename T>
T* List<T>::remove() {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_list.empty()) {
        return nullptr;
    }
    T* item = _list.front();
    _list.pop_front();
    return item;
}

template<typename T>
bool List<T>::empty() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _list.empty();
}

// Ordered_List implementations
template<typename T, typename R>
void Ordered_List<T, R>::insert(T* item) {
    std::lock_guard<std::mutex> lock(_mutex);
    _list.push_back(item);
}

template<typename T, typename R>
void Ordered_List<T, R>::remove(T* item) {
    std::lock_guard<std::mutex> lock(_mutex);
    _list.remove(item);
}

template<typename T, typename R>
typename Ordered_List<T, R>::Iterator Ordered_List<T, R>::begin() {
    return _list.begin();
}

template<typename T, typename R>
typename Ordered_List<T, R>::Iterator Ordered_List<T, R>::end() {
    return _list.end();
}

template<typename T, typename R>
bool Ordered_List<T, R>::empty() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _list.empty();
}

#endif // LIST_H
