#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstring>
#include "ethernet.h"

template<typename T>
class Message {
public:
    static constexpr std::size_t MAX_SIZE = Ethernet::MTU;

    Message();
    Message(const T data, std::size_t size);
    const T data() const;
    const std::size_t size() const;
    
private:
    T _data[MAX_SIZE];
    std::size_t _size;
};

template<typename T>
Message<T>::Message() : _size(0) { 
    memset(_data, 0, MAX_SIZE); 
}

template<typename T>
Message<T>::Message(const T data, std::size_t size) { 
    _size = (size > MAX_SIZE) ? MAX_SIZE : size; // Ensures that it does not exceed MTU
    memcpy(_data, data, size);
}

template<typename T>
const T Message<T>::data() const { 
    return _data; 
}

template<typename T>
const std::size_t Message<T>::size() const { 
    return _size; 
}

#endif // MESSAGE_H