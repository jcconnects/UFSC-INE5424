#ifndef BUFFER_H
#define BUFFER_H

#include <cstring>

template <typename T>
class Buffer{

    public:
        static constexpr unsigned int MAX_SIZE = sizeof(T);

    public:
        Buffer();
        Buffer(const void* data, unsigned int size);
        ~Buffer();

        T* data();
        void setData(const void* data, unsigned int size);
        const unsigned int size() const;
        void setSize(unsigned int size);
        void clear();

    private:
        std::uint8_t _data[MAX_SIZE];
        unsigned int _size;
};

template <typename T>
Buffer<T>::Buffer() : _size(0) {
    std::memset(_data, 0, MAX_SIZE);
}

template <typename T>
Buffer<T>::Buffer(const void* data, unsigned int size) {
    setData(data, size);
}

template <typename T>
Buffer<T>::~Buffer() {
    clear();
}

template <typename T>
T* Buffer<T>::data() {
    return reinterpret_cast<T*>(&_data);
}

template <typename T>
const unsigned int Buffer<T>::size() const {
    return _size;
}

template <typename T>
void Buffer<T>::setData(const void* data, unsigned int size) {
    setSize(size);
    std::memcpy(_data, data, _size);
}

template <typename T>
void Buffer<T>::setSize(unsigned int size) {
    _size = (size > MAX_SIZE) ? MAX_SIZE : size; // Ensures that it does not exceed MTU
}

template <typename T>
void Buffer<T>::clear() {
    std::memset(_data, 0, MAX_SIZE);
    _size = 0;
}

#endif // BUFFER_H