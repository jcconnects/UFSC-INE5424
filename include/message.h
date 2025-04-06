#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstring>

template <unsigned int MaxSize>
class Message {
    public:
        static constexpr const unsigned int MAX_SIZE = MaxSize;

        // Constructors
        Message();
        Message(const void* data, unsigned int size);
        
        // Copy constructor and assignment operator
        Message(const Message& other);
        Message& operator=(const Message& other);
        
        // Getters
        const void* data() const;
        const unsigned int size() const;
        
    private:
        void* _data[MAX_SIZE];
        unsigned int _size;
};


/******** Message Implementation ********/
template<unsigned int MaxSize>
Message<MaxSize>::Message() : _size(0) { 
    memset(_data, 0, MAX_SIZE); 
}

template<unsigned int MaxSize>
Message<MaxSize>::Message(const void* data, unsigned int size) { 
    _size = (size > MAX_SIZE) ? MAX_SIZE : size; // Ensures that it does not exceed Channel::MTU
    memcpy(_data, data, _size);
}

template<unsigned int MaxSize>
Message<MaxSize>::Message(const Message& other) : _size(other._size) {
    memcpy(_data, other._data, _size);
}

template<unsigned int MaxSize>
Message<MaxSize>& Message<MaxSize>::operator=(const Message& other) {
    if (this != &other) {
        _size = other._size;
        memcpy(_data, other._data, _size);
    }
    return *this;
}

template<unsigned int MaxSize>
const void* Message<MaxSize>::data() const { 
    return _data; 
}

template<unsigned int MaxSize>
const unsigned int Message<MaxSize>::size() const { 
    return _size; 
}

#endif // MESSAGE_H