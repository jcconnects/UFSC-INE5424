#include "observer.h"

template <typename T>
Semaphore::Semaphore(int count) : count(count) {}

template <typename T>
void Semaphore::p() {
    std::unique_lock<std::mutex> lock(mtx);
    while (count == 0) {
        cv.wait(lock);
    }
    count--;
}

template <typename T>
void Semaphore::v() {
    std::unique_lock<std::mutex> lock(mtx);
    count++;
    cv.notify_one();
}

template <typename T>
void List<T>::insert(T* item) {
    _items.push_back(item);
}

template <typename T>
T* List<T>::remove() {
    if (_items.empty())
        return nullptr;
    
    T* item = _items.front();
    _items.pop_front();
    return item;
}

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
Ordered_List<T, C>::Iterator& Ordered_List<T, C>::Iterator::operator++() {
    ++_it;
    return *this;
}

template <typename T, typename C>
bool Ordered_List<T, C>::Iterator::operator!=(const Iterator& other) {
    return _it != other._it;
}

template <typename T, typename C>
void Ordered_List<T, C>::insert(T* item) {
    _items.push_back(item);
}

template <typename T, typename C>
void Ordered_List<T, C>::remove(T* item) {
    _items.remove(item);
}

template <typename T, typename C>
Ordered_List<T, C>::Iterator Ordered_List<T, C>::begin() {
    return Iterator(_items.begin());
}

template <typename T, typename C>
Ordered_List<T, C>::Iterator Ordered_List<T, C>::end() {
    return Iterator(_items.end());
}

template <typename T, typename Condition>
void Conditional_Data_Observer<T, Condition>::update(Condition c, T* d) {
  // TODO: Implement
}

template <typename T, typename Condition>
Condition Conditional_Data_Observer<T, Condition>::rank() const {
  // TODO: Implement
}

template <typename T, typename C>
void Conditionally_Data_Observed<T, C>::attach(Conditional_Data_Observer<T, C>* o, C c) {
  _observers.insert(o);
}

template <typename T, typename C>
void Conditionally_Data_Observed<T, C>::detach(Conditional_Data_Observer<T, C>* o, C c) {
    _observers.remove(o);
}

template <typename T, typename C>
bool Conditionally_Data_Observed<T, C>::notify(C c, T * d) {
  bool notified = false;
  for (typename Observers::Iterator obs = _observers.begin(); obs != _observers.end(); ++obs) {
    if ((*obs)->rank() == c) {
      (*obs)->update(c, d);
      notified = true;
    }
  }
  return notified;
}

template <typename T, typename C>
void Concurrent_Observed<T, C>::attach(Concurrent_Observer<T, C> * o, C c) {
    _observers.insert(o);
}

template <typename T, typename C>
void Concurrent_Observed<T, C>::detach(Concurrent_Observer<T, C> * o, C c) {
    _observers.remove(o);
}

template <typename T, typename C>
bool Concurrent_Observed<T, C>::notify(C c, T * d) {
    bool notified = false;
    for (typename Observers::Iterator obs = _observers.begin(); obs != _observers.end(); ++obs) {
        if ((*obs)->rank() == c) {
            (*obs)->update(c, d);
            notified = true;
        }
    }
    return notified;
}

template <typename T, typename C>
void Concurrent_Observer<T, C>::update(C c, T * d) {
    _data.insert(d);
    _semaphore.v();
}

template <typename T, typename C>
T * Concurrent_Observer<T, C>::updated() {
    _semaphore.p();
    return _data.remove();
}

template <typename T, typename C>
C Concurrent_Observer<T, C>::rank() {
    return _rank;
}
