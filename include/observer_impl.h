#ifndef OBSERVER_IMPL_H
#define OBSERVER_IMPL_H

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

// Template implementations for Conditional_Data_Observer
template <typename T, typename Condition>
void Conditional_Data_Observer<T, Condition>::update(Condition c, T* d) {
    // TODO: Implement
}

template <typename T, typename Condition>
Condition Conditional_Data_Observer<T, Condition>::rank() const {
    // TODO: Implement
    return Condition();
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
bool Conditionally_Data_Observed<T, C>::notify(C c, T* d) {
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
void Concurrent_Observed<T, C>::attach(Concurrent_Observer<T, C>* o, C c) {
    std::lock_guard<std::mutex> lock(_mutex);
    _observers.insert(o);
}

template <typename T, typename C>
void Concurrent_Observed<T, C>::detach(Concurrent_Observer<T, C>* o, C c) {
    std::lock_guard<std::mutex> lock(_mutex);
    _observers.remove(o);
}

template <typename T, typename C>
bool Concurrent_Observed<T, C>::notify(C c, T* d) {
    std::lock_guard<std::mutex> lock(_mutex);
    bool notified = false;
    int observer_count = 0;

    // First count how many observers will receive this notification
    for (typename Observers::Iterator obs = _observers.begin(); obs != _observers.end(); ++obs) {
        if ((*obs)->rank() == c) {
            observer_count++;
        }
    }

    // Only set initial reference count if there are observers
    if (observer_count > 0 && d) {
        d->ref_count = observer_count;
        notified = true;
    }

    // Notify all observers that match the condition
    for (typename Observers::Iterator obs = _observers.begin(); obs != _observers.end(); ++obs) {
        if ((*obs)->rank() == c) {
            (*obs)->update(c, d);
        }
    }

    return notified;
}

template <typename T, typename C>
void Concurrent_Observer<T, C>::update(C c, T* d) {
    if (c == _rank) {
        // Increment reference count when adding to the observer's queue
        if (d) d->ref_count++;
        _data.insert(d);
        _semaphore.v();
    }
}

template <typename T, typename C>
T* Concurrent_Observer<T, C>::updated() {
    _semaphore.p();
    return _data.remove();
}

template <typename T, typename C>
C Concurrent_Observer<T, C>::rank() {
    return _rank;
}

#endif // OBSERVER_IMPL_H 