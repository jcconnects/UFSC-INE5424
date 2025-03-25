#ifndef ENGINE_h
#define ENGINE_h

#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/ether.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <memory>
#include "ethernet.h"
#include "buffer.h"

class SocketEngine {
public:
    static const size_t BUFFER_SIZE = 4096;

    SocketEngine();
    ~SocketEngine();

    void start();
    void stop();
    void send(Buffer * b);

private:
    int m_sockfd;
    int m_epollfd;
    bool m_running;
    uint8_t m_buffer[BUFFER_SIZE];

    void runEventLoop();
    void ProcessSocketEvent();
    void handleData(const uint8_t* data, size_t len);
};

// ==== Implementation =====
SocketEngine::SocketEngine() : m_sockfd(-1), m_epollfd(-1), m_running(false) {
    // seting up socket
    m_sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (m_sockfd < 0) {
        throw std::runtime_error("Socket creation failed: " + std::string(strerror(errno)));
    }

    // Set socket to non-blocking
    int flags = fcntl(m_sockfd, F_GETFL, 0);
    if (flags == -1 || fcntl(m_sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        close(m_sockfd);
        throw std::runtime_error("Failed to set non-blocking: " + std::string(strerror(errno)));
    }
    // seting up epoll
    m_epollfd = epoll_create1(0);
    if (m_epollfd < 0) {
        close(m_sockfd);
        throw std::runtime_error("Epoll creation failed: " + std::string(strerror(errno)));
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = m_sockfd;
    if (epoll_ctl(m_epollfd, EPOLL_CTL_ADD, m_sockfd, &ev) < 0) {
        close(m_sockfd);
        close(m_epollfd);
        throw std::runtime_error("Epoll ctl failed: " + std::string(strerror(errno)));
    }
}

SocketEngine::~SocketEngine() {
    stop();
    if (m_sockfd != -1) close(m_sockfd);
    if (m_epollfd != -1) close(m_epollfd);
}

void SocketEngine::start() {
    m_running = true;
    runEventLoop();
}

void SocketEngine::stop() {
    m_running = false;
}


void SocketEngine::send(Buffer * b) {
    // sends msg
}

void SocketEngine::runEventLoop() {
    const int MAX_EVENTS = 10;
    struct epoll_event events[MAX_EVENTS];

    while (m_running) {
        int nfds = epoll_wait(m_epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            if (errno == EINTR) continue;
            throw std::runtime_error("Epoll wait error: " + std::string(strerror(errno)));
        }

        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == m_sockfd) {
                ProcessSocketEvent();
            }
        }
    }
}

void SocketEngine::ProcessSocketEvent() {
    while (true) {  // process all available data
        ssize_t len = recv(m_sockfd, m_buffer, BUFFER_SIZE, 0);
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break; // breaks off the loop when there's no data to be processed
            throw std::runtime_error("Receive error: " + std::string(strerror(errno)));
        }
        if (len == 0) {
            throw std::runtime_error("Socket closed unexpectedly");
        }
        
        handleData(m_buffer, static_cast<size_t>(len));
    }
}

void SocketEngine::handleData(const uint8_t* data, size_t len) {
    // reconstructs instance of Ethernet::Frame
    Ethernet::Frame frame;
    std::memcpy(&frame, data, sizeof(Ethernet::Frame))

    // bufferize msg - TODO
}   

#endif // ENGINE_H