#ifndef SHAREDMEMORYENGINE_H
#define SHAREDMEMORYENGINE_H


#include "ethernet.h"
#include "buffer.h"
#include "traits.h"
#include "debug.h"


class SharedMemoryEngine {
    public:
        SharedMemoryEngine();
        ~SharedMemoryEngine();

        // Prevent copying
        SharedMemoryEngine(const SharedMemoryEngine&) = delete;
        SharedMemoryEngine& operator=(const SharedMemoryEngine&) = delete;

        void start();
        void stop();
        
        // --- Interface methods required by NIC ---
        // Send data into the shared queue
        int send(Buffer<Ethernet::Frame>* buf);

    private:
        virtual void handleInternal(Buffer<Ethernet::Frame>* buf) = 0;

};

/********** SharedMemoryEngine Implementation **********/

// Constructor: Initialize pointers/FDs to invalid states
SharedMemoryEngine::SharedMemoryEngine() {
    db<SharedMemoryEngine>(TRC) << "SharedMemoryEngine::SharedMemoryEngine() called!\n";
}

// Destructor: Ensure resources are released
SharedMemoryEngine::~SharedMemoryEngine() {
    db<SharedMemoryEngine>(TRC) << "SharedMemoryEngine::~SharedMemoryEngine() called!\n";
}

void SharedMemoryEngine::start() {
    db<SharedMemoryEngine>(TRC) << "SharedMemoryEngine::start() called!\n";
}

void SharedMemoryEngine::stop() {
    db<SharedMemoryEngine>(TRC) << "SharedMemoryEngine::stop() called!\n";
}

int SharedMemoryEngine::send(Buffer<Ethernet::Frame>* buf) {
    db<SharedMemoryEngine>(TRC) << "SharedMemoryEngine::send() called!\n";

    this->handleInternal(buf);

    return buf->size();
}

#endif // SHAREDMEMORYENGINE_H 