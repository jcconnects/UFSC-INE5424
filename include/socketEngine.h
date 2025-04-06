#ifndef SOCKETENGINE_H
#define SOCKETENGINE_H

#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <fcntl.h>
#include <iostream>
#include <sys/ioctl.h>
#include <functional>
#include <pthread.h>


#include "ethernet.h"
#include "traits.h"
#include "debug.h"

template <typename T>
class Buffer;

class SocketEngine{
    
    public:
        static constexpr const char* INTERFACE = Traits<SocketEngine>::INTERFACE_NAME;
        using CallbackMethod = std::function<void(Ethernet::Frame&, unsigned int)>;

    public:
        SocketEngine();

        ~SocketEngine();

        void setCallback(CallbackMethod callback);

        int send(Ethernet::Frame* frame, unsigned int size);

        static void* run(void* arg);

        void stop();

    private:
        void setUpSocket();

        void setUpEpoll();

        // Signal handler
        void handleSignal();

    protected:
        Ethernet::Address _address;
    
    private:
        int _sock_fd;
        int _ep_fd;
        int _if_index;

        CallbackMethod _callback;
        pthread_t _receive_thread;
        static bool _running;
};


/********** SocketEngine Implementation **********/

SocketEngine::SocketEngine()  {
    db<SocketEngine>(TRC) << "SocketEngine::SocketEngine() called!\n";
    setUpSocket();
    setUpEpoll();

    pthread_create(&_receive_thread, nullptr, SocketEngine::run, this);
    db<SocketEngine>(INF) << "[SocketEngine] receive thread started\n";
};

void SocketEngine::setUpSocket() {
    db<SocketEngine>(TRC) << "SocketEngine::setUpSocket() called!\n";

    // 1. Creating socket
    _sock_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (_sock_fd < 0) {
        perror("socket");
        throw std::runtime_error("Failed to create SocketEngine::_sock_fd!");
    }

    // 2. Making it non-blocking
    int flags = fcntl(_sock_fd, F_GETFL, 0);
    fcntl(_sock_fd, F_SETFL, flags | O_NONBLOCK);

    // 3. Getting interface index
    struct ifreq ifr;
    std::memset(&ifr, 0, sizeof(ifr));
    std::strncpy(ifr.ifr_name, INTERFACE, IFNAMSIZ);

    
    if (ioctl(_sock_fd, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl SIOCGIFINDEX");
        throw std::runtime_error("Failed to retrieve interface index!");
    }
    
    _if_index = ifr.ifr_ifindex;
    db<SocketEngine>(INF) << "[SocketEngine] if_index setted: " << _if_index << "\n";

    // 4. Getting MAC address
    std::memset(&ifr, 0, sizeof(ifr));
    std::strncpy(ifr.ifr_name, INTERFACE, IFNAMSIZ);

    if (ioctl(_sock_fd, SIOCGIFHWADDR, &ifr) < 0) {
        perror("ioctl SIOCGIFHWADDR");
        throw std::runtime_error("Failed to retrieve MAC address!");
    }

    std::memcpy(_address.bytes, ifr.ifr_hwaddr.sa_data, Ethernet::MAC_SIZE);
    db<SocketEngine>(INF) << "[SocketEngine] MAC address setted: " << Ethernet::mac_to_string(_address) << "\n";

    // 5. Bind socket to interface
    struct sockaddr_ll sll;
    std::memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_ALL);
    sll.sll_ifindex = _if_index;

    if (bind(_sock_fd, reinterpret_cast<struct sockaddr*>(&sll), sizeof(sll)) < 0) {
        perror("bind");
        throw std::runtime_error("Failed to bind SocketEngine::_sock_fd to interface!");
    }

    db<SocketEngine>(INF) << "[SocketEngine] socket setted\n";
}

void SocketEngine::setUpEpoll() {
    db<SocketEngine>(TRC) << "SocketEngine::setUpEpoll() called!\n";

    // 1. Creating epoll
    _ep_fd = epoll_create1(0);
    if (_ep_fd < 0) {
        perror("epoll_create1");
        throw std::runtime_error("Failed to create SocketEngine::_ep_fd!");
    }

    // 2. Binding socket on epoll
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = _sock_fd;

    if (epoll_ctl(_ep_fd, EPOLL_CTL_ADD, _sock_fd, &ev) < 0) {
        perror("epoll_ctl");
        throw std::runtime_error("Failed to bind SocketEngine::_sock_fd to SocketEngine::_ep_fd!");
    }

    db<SocketEngine>(INF) << "[SocketEngine] epoll setted\n";
}

SocketEngine::~SocketEngine()  {
    db<SocketEngine>(TRC) << "SocketEngine::~SocketEngine() called!\n";

    close(_sock_fd);
    close(_ep_fd);

    pthread_join(_receive_thread, nullptr);
    db<SocketEngine>(INF) << "[SocketEngine] receive thread finished\n";
};

int SocketEngine::send(Ethernet::Frame* frame, unsigned int size) {
    db<SocketEngine>(TRC) << "SocketEngine::send() called!\n";

    sockaddr_ll addr = {};
    addr.sll_family   = AF_PACKET;
    addr.sll_protocol = htons(frame->prot);
    addr.sll_ifindex  = _if_index;
    addr.sll_halen    = Ethernet::MAC_SIZE;
    std::memcpy(addr.sll_addr, frame->dst.bytes, Ethernet::MAC_SIZE);

    // Make sure protocol field is in network byte order before sending
    frame->prot = htons(frame->prot);

    int result = sendto(_sock_fd, frame, size, 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    db<SocketEngine>(INF) << "[SocketEngine] sendto() returned value " << std::to_string(result) << "\n";
    
    // Convert the protocol back to host order for logging
    frame->prot = ntohs(frame->prot);
    
    if (result < 0) {
        perror("sendto");
        db<SocketEngine>(ERR) << "[SocketEngine] Failed to send frame: {src = " << Ethernet::mac_to_string(frame->src) << ", dst = " << Ethernet::mac_to_string(frame->dst) << ", prot = " << std::to_string(frame->prot) << "}\n";
    } else {
        db<SocketEngine>(INF) << "[SocketEngine] Frame sent: {src = " << Ethernet::mac_to_string(frame->src) << ", dst = " << Ethernet::mac_to_string(frame->dst) << ", prot = " << std::to_string(frame->prot) << "}\n";
    }
    
    return result;
}

void SocketEngine::setCallback(CallbackMethod callback) {
    db<SocketEngine>(TRC) << "SocketEngine::setCallback() called!\n";
    _callback = callback;
}

void SocketEngine::handleSignal() {
    db<SocketEngine>(TRC) << "SocketEngine::handleSignal() called!\n";
    
    Ethernet::Frame frame;
    struct sockaddr_ll src_addr;
    socklen_t addr_len = sizeof(src_addr);
    
    int bytes_received = recvfrom(_sock_fd, &frame, sizeof(frame), 0, reinterpret_cast<sockaddr*>(&src_addr), &addr_len);
                               
    if (bytes_received < 0) {
        db<SocketEngine>(INF) << "[SocketEngine] No data received\n";
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("recvfrom");
        }
        return;
    }
    
    // Check for valid Ethernet frame size (at least header size)
    if (bytes_received < Ethernet::HEADER_SIZE) {
        db<SocketEngine>(ERR) << "[SocketEngine] Received undersized frame (" << bytes_received << " bytes)\n";
        return;
    }
    
    // Convert protocol from network to host byte order
    frame.prot = ntohs(frame.prot);
    db<SocketEngine>(INF) << "[SocketEngine] received frame: {src = " << Ethernet::mac_to_string(frame.src) << ", dst = " << Ethernet::mac_to_string(frame.dst) << ", prot = " << frame.prot << "}\n";
    
    // Process the frame if callback is set
    if (_callback) {
        _callback(frame, bytes_received);
    }
}

void* SocketEngine::run(void* arg)  {
    db<SocketEngine>(TRC) << "SocketEngine::run() called!\n";

    SocketEngine* engine = static_cast<SocketEngine*>(arg);

    struct epoll_event events[10];

    while (_running) {
        int n = epoll_wait(engine->_ep_fd, events, 10, 100); // 100ms timeout for check running
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd == engine->_sock_fd) {
                db<SocketEngine>(INF) << "[SocketEngine] epoll event detected\n";
                engine->handleSignal();
            }
        }
    }

    return nullptr;
};

void SocketEngine::stop() {
    db<SocketEngine>(TRC) << "SocketEngine::run() called!\n";
    SocketEngine::_running = false;
}

bool SocketEngine::_running = true;

#endif // SOCKETENGINE_H