#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstring>
#include "ethernet.h"

template<typename T>
class Message {

    public:
        static constexpr std::size_t MAX_SIZE = Ethernet::MTU;

        Message() : _size(0) { memset(_data, 0, MAX_SIZE); };

        Message(const T data, std::size_t size) { 
            _size = (size > MAX_SIZE) ? MAX_SIZE : size; // Ensures that it does not exceed MTU
            memcpy(_data, data, size);
        };
        
        const T data() const { return _data; };

        const std::size_t size() const { return _size; };
    
    private:
        T _data[MAX_SIZE];
        std::size_t _size;
};

#endif // MESSAGE_H