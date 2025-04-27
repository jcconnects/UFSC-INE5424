#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstring>
#include "component.h" // Include for TheAddress type alias (might need a dedicated types.h later)

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
        const void* data() const; // Returns const void* for generic access
        void* data();             // Returns void* for modification (use carefully)
        const unsigned int size() const;
        
        // Origin Address getter/setter
        const TheAddress& origin() const;
        void origin(const TheAddress& addr);
        
    private:
        // Store actual byte data, not an array of void pointers
        unsigned char _data[MAX_SIZE];
        unsigned int _size;
        TheAddress _origin; // Added origin address member
};


/******** Message Implementation ********/
template<unsigned int MaxSize>
Message<MaxSize>::Message() : _size(0), _origin() { // Initialize origin
    // Clear data buffer
    memset(_data, 0, MAX_SIZE);
}

template<unsigned int MaxSize>
Message<MaxSize>::Message(const void* data, unsigned int size)
    : _origin() // Initialize origin to default
{
    _size = (size > MAX_SIZE) ? MAX_SIZE : size; // Ensures that it does not exceed MAX_SIZE
    memcpy(_data, data, _size);
    // Clear remaining buffer space (optional, good practice)
    if (_size < MAX_SIZE) {
        memset(_data + _size, 0, MAX_SIZE - _size);
    }
}

template<unsigned int MaxSize>
Message<MaxSize>::Message(const Message& other)
    : _size(other._size),
      _origin(other._origin) // Copy origin
{
    memcpy(_data, other._data, MAX_SIZE); // Copy the whole buffer for simplicity
}

template<unsigned int MaxSize>
Message<MaxSize>& Message<MaxSize>::operator=(const Message& other) {
    if (this != &other) {
        _size = other._size;
        _origin = other._origin; // Assign origin
        memcpy(_data, other._data, MAX_SIZE); // Copy the whole buffer
    }
    return *this;
}

template<unsigned int MaxSize>
const void* Message<MaxSize>::data() const {
    return _data;
}

template<unsigned int MaxSize>
void* Message<MaxSize>::data() {
    return _data;
}

template<unsigned int MaxSize>
const unsigned int Message<MaxSize>::size() const {
    return _size;
}

template<unsigned int MaxSize>
const TheAddress& Message<MaxSize>::origin() const {
    return _origin;
}

template<unsigned int MaxSize>
void Message<MaxSize>::origin(const TheAddress& addr) {
    _origin = addr;
}

#endif // MESSAGE_H