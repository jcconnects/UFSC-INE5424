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
#include "../traits.h"
#include "../util/debug.h"


class SocketEngine{
    
    public:
        static const char* INTERFACE() { return Traits<SocketEngine>::INTERFACE_NAME(); }

    public:
        SocketEngine();

        virtual ~SocketEngine();

        void start();

        void stop();
        
        const bool running();
        
        int send(Ethernet::Frame* frame, unsigned int size);

        static void* run(void* arg);

        const Ethernet::Address& mac_address();

    private:
        void setUpSocket();

        void setUpEpoll();

        // This is the new epoll signal handler
        void receive();

        // Signal handler
        virtual void handle(Ethernet::Frame* frame, unsigned int size) = 0;

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

SocketEngine::SocketEngine() : _stop_ev(eventfd(0, EFD_NONBLOCK)), _running(false) {
    // Do NOT auto-start - let NIC control when to start
    // Initialize socket and epoll setup only
    setUpSocket();
    setUpEpoll();
    db<SocketEngine>(INF) << "[SocketEngine] constructor completed - ready to start\n";
};

SocketEngine::~SocketEngine()  {
    db<SocketEngine>(TRC) << "SocketEngine::~SocketEngine() called!\n";

    stop();

    close(_sock_fd);
    close(_ep_fd);
    close(_stop_ev); // Also close the eventfd
};

void SocketEngine::start() {
    db<SocketEngine>(TRC) << "SocketEngine::start() called!\n";

    if (_running.load()) {
        db<SocketEngine>(WRN) << "[SocketEngine] Already running, ignoring start() call\n";
        return;
    }
    
    // Socket and epoll are already set up in constructor
    // Just start the receive thread
    _running.store(true, std::memory_order_release);
    pthread_create(&_receive_thread, nullptr, SocketEngine::run, this);
    
    db<SocketEngine>(INF) << "[SocketEngine] receive thread started\n";
}

void SocketEngine::stop() {
    db<SocketEngine>(TRC) << "SocketEngine::stop() called!\n";
    
    if (!running()) return;

    _running.store(false, std::memory_order_release);

    std::uint64_t u = 1;
    ssize_t bytes_written;
    db<SocketEngine>(TRC) << "[SocketEngine] sending stop signal to receive thread\n";
    do {
        bytes_written = write(_stop_ev, &u, sizeof(u));
    } while (bytes_written == -1 && errno == EINTR); // Retry only on EINTR
    db<SocketEngine>(TRC) << "[SocketEngine] stop signal sent to receive thread\n";

    // Join the receive thread if it exists
    if (_receive_thread != 0) {
        int ret = pthread_join(_receive_thread, nullptr);
        if (ret == 0) {
            db<SocketEngine>(INF) << "[SocketEngine] successfully stopped!\n";
        } else {
            db<SocketEngine>(ERR) << "[SocketEngine] failed to join thread with error: " << ret << "\n";
        }
    } else {
        db<SocketEngine>(ERR) << "[SocketEngine] receive thread is not running!\n";
    }

    db<SocketEngine>(INF) << "[SocketEngine] sucessfully stopped!\n";
}

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

int SocketEngine::send(Ethernet::Frame* frame, unsigned int size) {
    db<SocketEngine>(TRC) << "SocketEngine::send() called!\n";

    // Check if engine is running before sending
    if (!running()) {
        db<SocketEngine>(ERR) << "[SocketEngine] Attempted to send while engine is stopping/stopped\n";
        return -1;
    }

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
    
    return result;
}

void* SocketEngine::run(void* arg)  {
    db<SocketEngine>(TRC) << "[SocketEngine] [run()] called!\n";

    SocketEngine* engine = static_cast<SocketEngine*>(arg);

    struct epoll_event events[1024];

    while (engine->running()) {
        db<SocketEngine>(TRC) << "[SocketEngine] [run()] epoll_wait() called\n";
        int n = epoll_wait(engine->_ep_fd, events, 1024, -1);

        db<SocketEngine>(TRC) << "[SocketEngine] [run()] epoll event detected\n";
        
        if (n < 0) {
            db<SocketEngine>(TRC) << "[SocketEngine] [run()] epoll_wait() returned error: " << errno << "\n";
            if (errno == EINTR) continue;
            db<SocketEngine>(TRC) << "[SocketEngine] [run()] epoll_wait() returned error: " << errno << "\n";
            perror("epoll_wait");
            break;
        }

        // Iterates over all events detected by epoll
        for (int i = 0; i < n; ++i) {
            db<SocketEngine>(TRC) << "[SocketEngine] [run()] epoll event " << i << " detected\n";
            int fd = events[i].data.fd;

            if (fd == engine->_sock_fd) {
                db<SocketEngine>(INF) << "[SocketEngine] [run()] epoll socket event detected\n";
                engine->receive();
                db<SocketEngine>(TRC) << "[SocketEngine] [run()] receive() called\n";
            } else if (fd == engine->_stop_ev) {
                db<SocketEngine>(INF) << "[SocketEngine] [run()] epoll stop event detected\n";
                uint64_t u;
                read(engine->_stop_ev, &u, sizeof(u)); // clears eventfd
                db<SocketEngine>(TRC) << "[SocketEngine] [run()] stop event cleared\n";
                break; // Next loop, thread will finish
            }
        }
    }

    db<SocketEngine>(INF) << "[SocketEngine] [run()] receive thread terminated!\n";
    return nullptr;
};

void SocketEngine::receive() {
    db<SocketEngine>(TRC) << "[SocketEngine] [receive()] called!\n";

    // Checks weather engine is still active
    if (!running()) {
        db<SocketEngine>(ERR) << "[SocketEngine] [receive()] called when engine is inactive\n";
        return;
    }

    Ethernet::Frame frame;
    struct sockaddr_ll src_addr;
    socklen_t addr_len = sizeof(src_addr);
    
    int bytes_received = recvfrom(this->_sock_fd, &frame, sizeof(frame), 0, reinterpret_cast<sockaddr*>(&src_addr), &addr_len);
    
    // Checks weather receive was sucessful
    if (bytes_received < 0) {
        db<SocketEngine>(INF) << "[SocketEngine] [receive()] no data received\n";
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("recvfrom");
        }
        return;
    }

    // Checks for valid Ethernet frame size (at least header size)
    if (static_cast<unsigned int>(bytes_received) < Ethernet::HEADER_SIZE) {
        db<SocketEngine>(ERR) << "[SocketEngine] [receive()] Received undersized frame (" << bytes_received << " bytes)\n";
        return;
    }
    
    // Convert protocol from network to host byte order
    frame.prot = ntohs(frame.prot);
    db<SocketEngine>(INF) << "[SocketEngine] [receive()] received frame: {src = " << Ethernet::mac_to_string(frame.src) << ", dst = " << Ethernet::mac_to_string(frame.dst) << ", prot = " << frame.prot << ", size = " << bytes_received << "}\n";

    this->handle(&frame, static_cast<unsigned int>(bytes_received));
}

const bool SocketEngine::running() {
    return _running.load(std::memory_order_acquire);
}

const Ethernet::Address& SocketEngine::mac_address() {
    return _mac_address;
}

#endif // SOCKETENGINE_H