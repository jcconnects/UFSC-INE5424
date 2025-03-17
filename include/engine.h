#ifndef NETWORK_H
#define NETWORK_H

#include "observer.h"
#include "protocol.h"
#include "network.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdexcept>

class Buffer
{
public:
    Buffer(Address dst, Protocol_Number prot, unsigned int size);
    ~Buffer();
    getBuffer();
    getSizeOfBuffer();
    appendToBuffer();
    eraseData();
private:
    char * buf;
}

// Engine Interface
class SocketEngine 
{
public:
    SocketEngine() : socket_raw(-1) {
        // Create a raw socket
        socket_raw = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
        if (socket_raw < 0) {
            throw std::runtime_error("Failed to create raw socket");
        }

        // Set the socket to non-blocking mode
        int flags = fcntl(socket_raw, F_GETFL, 0);
        if (fcntl(socket_raw, F_SETFL, flags | O_NONBLOCK) < 0) {
            close(socket_raw);
            throw std::runtime_error("Failed to set socket to non-blocking mode");
        }

        // Enable asynchronous I/O using SIGIO
        if (fcntl(socket_raw, F_SETOWN, getpid()) < 0) {
            close(socket_raw);
            throw std::runtime_error("Failed to set socket owner");
        }

        int opts = fcntl(socket_raw, F_GETFL);
        if (fcntl(socket_raw, F_SETFL, opts | O_ASYNC) < 0) {
            close(socket_raw);
            throw std::runtime_error("Failed to enable asynchronous I/O");
        }

        // Set up a signal handler for SIGIO
        signal(SIGIO, &SocketEngine::handleSigio);
    }

    ~SocketEngine() {
        if (socket_raw >= 0) {
            close(socket_raw);
        }
    }

    send(Buffer * b) {
        sockaddr_in destAddr;
        memset(&destAddr, 0, sizeof(destAddr));
        destAddr.sin_family = AF_INET;
        /*
        if (inet_pton(AF_INET, ip, &destAddr.sin_addr) <= 0) {
            throw std::runtime_error("Invalid IP address");
        }
        */
        ssize_t bytes_sent = sendto(socket_raw, b.getBuffer(), b.getSizeOfBuffer(), (struct sockaddr *) &destAddr, sizeof(destAddr));
        if (bytes_sent < 0) throw std::runtime_error("Failed to send data");
    }

    // Static signal handler for SIGIO
    static void handleSigio(int sig) {
        if (sig == SIGIO) {
            std::cout << "SIGIO received: Asynchronous I/O event occurred" << std::endl;
            char b[1518];
            int n = recvfrom(socket_raw, b, sizeof(b), 0, NULL, NULL);
            // int receive(Buffer * buf, Address * src, Address * dst, void * data, unsigned int size);
            /*
            extract headers
            initialize necessary objects: buffer, src address, dst address
            make _nic -> receive(); call
            */
        }
    }

private:
    int socket_raw;
    NIC * _nic;
}

#endif // NETWORK_H