#ifndef SOCKETENGINE_H
#define SOCKETENGINE_H

#include <string>
#include <iostream>

// A simple SocketEngine class that simulates network operations
class SocketEngine {
public:
    SocketEngine();
    ~SocketEngine();
    
    // Simulates sending data over a socket
    int send(const void* data, unsigned int size);
    
    // Simulates receiving data from a socket
    int receive(void* data, unsigned int size);
    
    // Get local address
    std::string getLocalAddress() const;
};

// SocketEngine implementations
SocketEngine::SocketEngine() {
    std::cout << "SocketEngine created" << std::endl;
}

SocketEngine::~SocketEngine() {
    std::cout << "SocketEngine destroyed" << std::endl;
}

int SocketEngine::send(const void* data, unsigned int size) {
    // Simulate successful transmission
    std::cout << "SocketEngine sending " << size << " bytes" << std::endl;
    return size;
}

int SocketEngine::receive(void* data, unsigned int size) {
    // Simulate successful reception (actual data would be filled by the OS)
    std::cout << "SocketEngine receiving up to " << size << " bytes" << std::endl;
    return size;
}

std::string SocketEngine::getLocalAddress() const {
    return "local_address";
}

#endif // SOCKETENGINE_H 