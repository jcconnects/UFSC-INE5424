#ifndef SHAREDMEMORYENGINE_H
#define SHAREDMEMORYENGINE_H

#include "ethernet.h"

// Forward declaration
template <typename T>
class Buffer;

class SharedMemoryEngine {
    public:
        SharedMemoryEngine() {};
        virtual ~SharedMemoryEngine() {};

        virtual int send(Buffer<Ethernet::Frame>* buf) = 0;
};

/********** SharedMemoryEngine Implementation **********/


#endif // SHAREDMEMORYENGINE_H 