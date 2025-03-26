#ifndef BUFFER_H
#define BUFFER_H

#include <cstring>
#include "ethernet.h"

template <typename T>
class Buffer{

    public:
        static constexpr size_t MAX_SIZE = Ethernet::MTU;

    public:
        Buffer();

        Buffer(const void* data, std::size_t size);

        ~Buffer();

        const T* data() const;

        void setData(const void* data, size_t size);

        const std::size_t size() const;

        void setSize(std::size_t size);

        void clear();

    private:
        std::uint8_t _data[MAX_SIZE];
        std::size_t _size;
};


/********* Buffer Implementation **********/
template <typename T>
Buffer<T>::Buffer() : _size(0) {
    std::memset(_data, 0, MAX_SIZE);
}

template <typename T>
Buffer<T>::Buffer(const void* data, std::size_t size) {
    setData(data, size);
}

template <typename T>
Buffer<T>::~Buffer() {
    clear();
}

template <typename T>
const T* Buffer<T>::data() const {
    return reinterpret_cast<T*>(_data);
}

template <typename T>
void Buffer<T>::setData(const void* data, std::size_t size) {
    setSize(size);
    std::memcpy(_data, data, _size);
}

template <typename T>
const std::size_t Buffer<T>::size() const {
    return _size;
}

template <typename T>
void Buffer<T>::setSize(std::size_t size) {
    _size = (size > MAX_SIZE) ? MAX_SIZE : size; // Ensures that it does not exceed MTU
}

template <typename T>
void Buffer<T>::clear() {
    std::memset(_data, 0, MAX_SIZE);
    _size = 0;
}

#endif // BUFFER_H