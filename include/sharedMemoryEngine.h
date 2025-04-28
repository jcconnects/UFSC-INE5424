#ifndef SHAREDMEMORYENGINE_H
#define SHAREDMEMORYENGINE_H

#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>    // For mode constants
#include <fcntl.h>       // For O_* constants
#include <semaphore.h>
#include <sys/timerfd.h> // For timerfd
#include <time.h>        // For timespec
#include <atomic>
#include <stdexcept>
#include <string>
#include <iostream> // Include missing iostream

#include "ethernet.h"
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
    // Send data (payload + protocol) into the shared queue
    int send(Ethernet::Frame* frame, unsigned int size);
    // Get the MAC address of the engine
    Ethernet::Address getMacAddress() const;
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

int SharedMemoryEngine::send(Ethernet::Frame* frame, unsigned int size) {
    db<SharedMemoryEngine>(TRC) << "SharedMemoryEngine::send() called!\n";

    return size;
}

Ethernet::Address SharedMemoryEngine::getMacAddress() const {
    db<SharedMemoryEngine>(TRC) << "SharedMemoryEngine::getMacAddress() called.\n";
    return Ethernet::NULL_ADDRESS; // Internal engine doesn't have a specific external MAC
}

#endif // SHAREDMEMORYENGINE_H 