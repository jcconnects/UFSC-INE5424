#ifndef SOCKETENGINE_H
#define SOCKETENGINE_H

#include <string>
#include <iostream>

// A simple SocketEngine class that simulates network operations
class SocketEngine {
public:
    SocketEngine() {
        std::cout << "SocketEngine created" << std::endl;
    }
    
    ~SocketEngine() {
        std::cout << "SocketEngine destroyed" << std::endl;
    }
    
    // Simulates sending data over a socket
    int send(const void* data, unsigned int size) {
        // Simulate successful transmission
        std::cout << "SocketEngine sending " << size << " bytes" << std::endl;
        return size;
    }
    
    // Simulates receiving data from a socket
    int receive(void* data, unsigned int size) {
        // Simulate successful reception (actual data would be filled by the OS)
        std::cout << "SocketEngine receiving up to " << size << " bytes" << std::endl;
        return size;
    }
    
    // Get local address
    std::string getLocalAddress() const {
        return "local_address";
    }
};

#endif // SOCKETENGINE_H 