#include "socket_engine.h"
#include <stdexcept>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <iostream>

SocketEngine* SocketEngine::activeInstance = nullptr;
// ===== Implementation =====
SocketEngine::SocketEngine() : _socket(-1) {
    // Create raw socket
    _socket = socket(AF_PACKET, SOCK_RAW, htons(0x1234));
    if (_socket < 0) {
        throw std::runtime_error("socket creation failed");
    }

    // Set non-blocking mode
    int flags = fcntl(_socket, F_GETFL, 0);
    if (fcntl(_socket, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(_socket);
        throw std::runtime_error("fcntl(O_NONBLOCK) failed");
    }

    // Set process to receive SIGIO
    if (fcntl(_socket, F_SETOWN, getpid()) < 0) {
        close(_socket);
        throw std::runtime_error("fcntl(F_SETOWN) failed");
    }

    // Enable async I/O
    flags = fcntl(_socket, F_GETFL);
    if (fcntl(_socket, F_SETFL, flags | O_ASYNC) < 0) {
        close(_socket);
        throw std::runtime_error("fcntl(O_ASYNC) failed");
    }

    // Set up signal handler
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &SocketEngine::signalHandler;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGIO);
    
    if (sigaction(SIGIO, &sa, nullptr) < 0) {
        close(_socket);
        throw std::runtime_error("sigaction failed");
    }

    ifindex = if_nametoindex("eth0");
    if (ifindex == 0) {
        close(_socket);
        throw std::runtime_error("Interface eth0 not found");
    }

    struct sockaddr_ll addr{};
    memset(&addr, 0, sizeof(addr));
    addr.sll_family = AF_PACKET;
    addr.sll_ifindex = ifindex;
    addr.sll_protocol = htons(0x1234);

    if (bind(_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        throw std::runtime_error("bind failed: " + std::string(strerror(errno)));
    }
    activeInstance = this;
}

SocketEngine::~SocketEngine() {
    if (_socket != -1) close(_socket);
    activeInstance = nullptr;
}

void SocketEngine::signalHandler(int) {
    if (activeInstance) activeInstance -> asyncReceive();
}

void SocketEngine::asyncReceive() {
    sockaddr_in src_addr{};
    socklen_t addrlen = sizeof(src_addr);
    char buffer[1518];

    while (true) {
        std::size_t received = recvfrom(_socket, buffer, sizeof(buffer), 0,
                                    reinterpret_cast<sockaddr*>(&src_addr), &addrlen);
        if (received <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            if (received == 0) break;
            throw std::runtime_error("recvfrom error");
        }
        
        _cb(buffer, received);
    }
}

void SocketEngine::setCallback(callbackMethod cb) {
    _cb = cb;
}

int SocketEngine::send(const void* data, std::size_t length) {
    // Get source MAC address
    uint8_t src_mac[6];
    struct ifreq ifr{};
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, "eth0", IFNAMSIZ);
    if (ioctl(_socket, SIOCGIFINDEX, &ifr) < 0) {
        throw std::runtime_error("ioctl(SIOCGIFINDEX) failed");
    }
    ifindex = ifr.ifr_ifindex;
    if (ioctl(_socket, SIOCGIFINDEX, &ifr) < 0) {
        throw std::runtime_error("ioctl(SIOCGIFINDEX) failed");
    }
    strncpy(ifr.ifr_name, ifr.ifr_name, IFNAMSIZ);
    if (ioctl(_socket, SIOCGIFHWADDR, &ifr) < 0) {
        throw std::runtime_error("ioctl(SIOCGIFHWADDR) failed");
    }
    memcpy(src_mac, ifr.ifr_hwaddr.sa_data, 6);

    // Construct frame
    uint8_t frame[sizeof(EthFrame) + length];
    EthFrame* header = reinterpret_cast<EthFrame*>(frame);
    
    // Set destination MAC to broadcast address
    memset(header->dest_mac, 0xFF, 6);
    memcpy(header->src_mac, src_mac, 6);
    header->eth_type = htons(0x1234);
    memcpy(frame + sizeof(EthFrame), data, length);

    // Prepare destination address structure
    struct sockaddr_ll dest_addr{};
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sll_family = AF_PACKET;
    dest_addr.sll_ifindex = ifindex;
    dest_addr.sll_halen = ETH_ALEN;

    std::size_t bytes_sent = sendto(_socket, frame, sizeof(frame), 0,
                            reinterpret_cast<struct sockaddr*>(&dest_addr),
                            sizeof(dest_addr));

    if (bytes_sent < 0) {
        throw std::runtime_error(std::string("sendto failed: ") + strerror(errno));
    }
    // In SocketEngine::send
    std::cout << "Sent " << bytes_sent << " bytes\n";
    return bytes_sent;
}