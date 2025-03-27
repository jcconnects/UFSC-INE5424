#ifndef ENGINE_h
#define ENGINE_h

#include <sys/socket.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <functional>
#include <stdexcept>
#include <cstring>


class SocketEngine {
public:

    SocketEngine();
    ~SocketEngine();

    void send(const void* data, std::size_t length);

protected:
    virtual void receive(const void* data, std::size_t size); // override in derived class NIC

private:
    int _socket;

    static void signalHandler();
}

// ===== Implementation =====
SocketEngine::SocketEngine() : _socket(-1) {
    // Create raw socket
    _socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
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
}

SocketEngine::~SocketEngine() {
    if (_socket != -1) close(_socket);
}

void SocketEngine::signalHandler() {
    sockaddr_in src_addr{};
    socklen_t addrlen = sizeof(src_addr);
    char buffer[1518];

    while (true) {
        ssize_t received = recvfrom(_socket, buffer, sizeof(buffer), 0,
                                    reinterpret_cast<sockaddr*>(&src_addr), &addrlen);
        if (received <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            if (received == 0) break;
            throw std::runtime_error("recvfrom error");
        }
        
        receive(buffer, received);
    }
}

void SocketEngine::send(const void* data, size_t length) {
    // Ethernet frame structure
    struct EthFrame {
        uint8_t dest_mac[6];
        uint8_t src_mac[6];
        uint16_t eth_type;
        uint8_t payload[];
    } __attribute__((packed));

    // Get source MAC address
    uint8_t src_mac[6];
    struct ifreq ifr{};
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_ifindex = ifindex;
    if (ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0) {
        throw std::runtime_error("ioctl(SIOCGIFINDEX) failed");
    }
    strncpy(ifr.ifr_name, ifr.ifr_name, IFNAMSIZ);
    if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) < 0) {
        throw std::runtime_error("ioctl(SIOCGIFHWADDR) failed");
    }
    memcpy(src_mac, ifr.ifr_hwaddr.sa_data, 6);

    // Construct frame
    uint8_t frame[sizeof(EthFrame) + data_len];
    EthFrame* header = reinterpret_cast<EthFrame*>(frame);
    
    // Set destination MAC to broadcast address
    memset(header->dest_mac, 0xFF, 6);
    memcpy(header->src_mac, src_mac, 6);
    header->eth_type = htons(ETH_P_ARP);
    memcpy(frame + sizeof(EthFrame), data, data_len);

    // Prepare destination address structure
    struct sockaddr_ll dest_addr{};
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sll_family = AF_PACKET;
    dest_addr.sll_ifindex = ifindex;
    dest_addr.sll_halen = ETH_ALEN;

    if (sendto(sockfd, frame, sizeof(frame), 0,
                reinterpret_cast<struct sockaddr*>(&dest_addr),
                sizeof(dest_addr)) < 0) {
        throw std::runtime_error("sendto failed");
    }
}

#endif // ENGINE_H