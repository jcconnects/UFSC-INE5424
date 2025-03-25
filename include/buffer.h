#ifndef BUFFER_H
#define BUFFER_H

#include <cstring>
#include "ethernet.h"

template <typename T>
class Buffer{

    public:
        static constexpr size_t MAX_SIZE = Ethernet::MTU;

    public:
        Buffer() : _size(0) {
            memset(_data, 0, MAX_SIZE);
        };

        Buffer(const void* data, size_t size) {
            setData(data, size);
        };

        ~Buffer() = default;

        const T* data() const { return reinterpret_cast<T*>(_data); };

        void setData(const void* data, size_t size) {
            setSize(size);
            memcpy(_data, data, _size);
        };

        const size_t size() const { return _size; };

        void setSize(size_t size) {
            _size = (size > MAX_SIZE) ? MAX_SIZE : size; // Ensures that it does not exceed MTU
        };

        void clear() {
            memset(_data, 0, MAX_SIZE);
            _size = 0;
        };

    private:
        uint8_t _data[MAX_SIZE];
        size_t _size;
};

#endif // BUFFER_H