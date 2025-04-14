#ifndef SOCKETENGINE_H
#define SOCKETENGINE_H

#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <netinet/in.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <fcntl.h>
#include <iostream>
#include <sys/ioctl.h>
#include <functional>
#include <pthread.h>
#include <atomic>


#include "ethernet.h"
#include "traits.h"
#include "debug.h"

template <typename T>
class Buffer;

class SocketEngine{
    
    public:
        static const char* INTERFACE() { return Traits<SocketEngine>::INTERFACE_NAME(); }

    public:
        SocketEngine();

        virtual ~SocketEngine();

        const bool running();
        
        int send(Ethernet::Frame* frame, unsigned int size);

        static void* run(void* arg);

        void stop();


    private:
        void setUpSocket();

        void setUpEpoll();

        // Signal handler
        virtual void handleSignal() = 0;

    protected:
        int _sock_fd;
        int _ep_fd;
        int _if_index;
        Ethernet::Address _mac_address;
        
    private:
        const int _stop_ev;
        pthread_t _receive_thread;
        std::atomic<bool> _running;
};


/********** SocketEngine Implementation **********/

SocketEngine::SocketEngine() : _stop_ev(eventfd(0, EFD_NONBLOCK)){
    db<SocketEngine>(TRC) << "SocketEngine::SocketEngine() called!\n";
    setUpSocket();
    setUpEpoll();

    _running.store(true, std::memory_order_release);
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
    std::strncpy(ifr.ifr_name, INTERFACE(), IFNAMSIZ);

    
    if (ioctl(_sock_fd, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl SIOCGIFINDEX");
        throw std::runtime_error("Failed to retrieve interface index!");
    }
    
    _if_index = ifr.ifr_ifindex;
    db<SocketEngine>(INF) << "[SocketEngine] if_index setted: " << _if_index << "\n";

    // 4. Getting MAC address
    std::memset(&ifr, 0, sizeof(ifr));
    std::strncpy(ifr.ifr_name, INTERFACE(), IFNAMSIZ);

    if (ioctl(_sock_fd, SIOCGIFHWADDR, &ifr) < 0) {
        perror("ioctl SIOCGIFHWADDR");
        throw std::runtime_error("Failed to retrieve MAC address!");
    }

    std::memcpy(_mac_address.bytes, ifr.ifr_hwaddr.sa_data, Ethernet::MAC_SIZE);
    db<SocketEngine>(INF) << "[SocketEngine] MAC address setted: " << Ethernet::mac_to_string(_mac_address) << "\n";

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
        throw std::runtime_error("Failed to bind SocketEngine::_sock_fd to epoll!");
    }

    // 3. Binding stop event on epoll
    struct epoll_event stop_ev = {};
    stop_ev.events = EPOLLIN;
    stop_ev.data.fd = _stop_ev;
    if (epoll_ctl(_ep_fd, EPOLL_CTL_ADD, _stop_ev, &stop_ev) < 0) {
        perror("epoll_ctl stop_ev");
        throw std::runtime_error("Failed to bind SocketEngine::_stop_ev to epoll!");
    }

    db<SocketEngine>(INF) << "[SocketEngine] epoll setted\n";
}

SocketEngine::~SocketEngine()  {
    db<SocketEngine>(TRC) << "SocketEngine::~SocketEngine() called!\n";

    // Ensure the thread is stopped and joined before closing resources
    stop();

    close(_sock_fd);
    close(_ep_fd);
    close(_stop_ev); // Also close the eventfd
};

const bool SocketEngine::running() {
    return _running.load(std::memory_order_acquire);
}

int SocketEngine::send(Ethernet::Frame* frame, unsigned int size) {
    db<SocketEngine>(TRC) << "SocketEngine::send() called!\n";

    sockaddr_ll addr = {};
    addr.sll_family   = AF_PACKET;
    addr.sll_protocol = htons(frame->prot);
    addr.sll_ifindex  = _if_index;
    addr.sll_halen    = Ethernet::MAC_SIZE;
    std::memcpy(addr.sll_addr, _mac_address.bytes, Ethernet::MAC_SIZE);

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

void* SocketEngine::run(void* arg)  {
    db<SocketEngine>(TRC) << "SocketEngine::run() called!\n";

    SocketEngine* engine = static_cast<SocketEngine*>(arg);

    struct epoll_event events[10];
    while (engine->running()) {
        int n = epoll_wait(engine->_ep_fd, events, 10, -1);
        
        // Check if we should exit after epoll_wait returns
        if (!engine->running()) {
            db<SocketEngine>(TRC) << "[SocketEngine] running is false after epoll_wait, exiting loop.\n";
            break;
        }
        
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n; ++i) {
            // Check running state again before handling any event
            if (!engine->running()) {
                db<SocketEngine>(TRC) << "[SocketEngine] running is false during event processing, exiting loop.\n";
                break; // Exit loop if stopped during or after epoll_wait
            }
            
            int fd = events[i].data.fd;

            if (fd == engine->_sock_fd) {
                db<SocketEngine>(INF) << "[SocketEngine] epoll socket event detected\n";
                engine->handleSignal();
            } else if (fd == engine->_stop_ev) {
                db<SocketEngine>(INF) << "[SocketEngine] epoll stop event detected\n";
                uint64_t u;
                read(engine->_stop_ev, &u, sizeof(u)); // clears eventfd
            }
        }
    }

    db<SocketEngine>(INF) << "[SocketEngine] receive thread terminated!\n";
    return nullptr;
};

void SocketEngine::stop() {
    db<SocketEngine>(TRC) << "SocketEngine::stop() called!\n";
    
    // Atomically set flag to false and check if it was already false
    if (!_running.exchange(false, std::memory_order_acq_rel)) {
        db<SocketEngine>(TRC) << "[SocketEngine] Stop called but already stopped.\n";
        return; // Return if it was already false
    }

    if (!running())
        db<SocketEngine>(TRC) << "[SocketEngine] _running set to false.\n";

    std::uint64_t u = 1;
    ssize_t bytes_written = write(_stop_ev, &u, sizeof(u));

    db<SocketEngine>(TRC) << "[SocketEngine] " << bytes_written << " bytes written.\n";

    pthread_join(_receive_thread, nullptr);
    db<SocketEngine>(INF) << "[SocketEngine] sucessfully stopped!\n";
}

#endif // SOCKETENGINE_H