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
#include <cstdint>
#include <stdexcept> // Added for runtime_error

#include "ethernet.h"
#include "traits.h"
#include "debug.h"

// Forward declaration removed, no longer needed
// template <typename T>
// class Buffer;

class SocketEngine{

    public:
        static const char* INTERFACE() { return Traits<SocketEngine>::INTERFACE_NAME(); }

    public:
        SocketEngine();

        ~SocketEngine(); // Changed to non-virtual, no longer base for NIC

        void start();

        void stop();

        const bool running() const; // Added const

        // Send an Ethernet frame
        int send(Ethernet::Frame* frame, unsigned int size);

        // Receive an Ethernet frame (called by NIC)
        int receiveFrame(Ethernet::Frame& frame_buffer);

        // --- Interface methods required by NIC ---
        Ethernet::Address getMacAddress() const;
        int getNotificationFd() const;
        // ----------------------------------------

    private:
        void setUpSocket();
        // setUpEpoll removed
        // handleSignal removed
        // run removed

    protected: // Changed to private as NIC no longer inherits
        int _sock_fd;
        // _ep_fd removed
        int _if_index;
        Ethernet::Address _mac_address;

    private:
        // _stop_ev removed
        // _receive_thread removed
        std::atomic<bool> _running;
};


/********** SocketEngine Implementation **********/

// Constructor: Initialize members, setup socket
SocketEngine::SocketEngine() : _sock_fd(-1), _if_index(-1), _running(false) {
    db<SocketEngine>(TRC) << "SocketEngine::SocketEngine() called!\n";
    // Socket setup moved to start() to allow object creation before interface might be ready
};

// Start: Setup the raw socket
void SocketEngine::start() {
    db<SocketEngine>(TRC) << "SocketEngine::start() called!\n";
    if (running()) {
        db<SocketEngine>(WRN) << "SocketEngine::start() called but already running.\n";
        return;
    }
    try {
        setUpSocket();
        _running.store(true, std::memory_order_release);
        db<SocketEngine>(INF) << "[SocketEngine] Started and socket ready (fd=" << _sock_fd << ").\n";
    } catch (const std::exception& e) {
        db<SocketEngine>(ERR) << "SocketEngine::start() failed: " << e.what() << "\n";
        // Ensure socket is closed if partially opened
        if (_sock_fd >= 0) {
            close(_sock_fd);
            _sock_fd = -1;
        }
        _running.store(false);
        throw; // Rethrow the exception
    }
}

// Stop: Close the socket and set running state to false
void SocketEngine::stop() {
    db<SocketEngine>(TRC) << "SocketEngine::stop() called!\n";

    if (!running()) {
         db<SocketEngine>(INF) << "[SocketEngine] stop() called but not running.\n";
         return;
    }

    _running.store(false, std::memory_order_release);

    if (_sock_fd >= 0) {
        close(_sock_fd);
        _sock_fd = -1;
        db<SocketEngine>(INF) << "[SocketEngine] Socket closed.\n";
    } else {
         db<SocketEngine>(WRN) << "[SocketEngine] stop() called but socket FD was invalid.\n";
    }

    db<SocketEngine>(INF) << "[SocketEngine] Stopped.\n";
}

// Setup the raw socket (modified from original)
void SocketEngine::setUpSocket() {
    db<SocketEngine>(TRC) << "SocketEngine::setUpSocket() called!\n";

    // 1. Creating socket
    _sock_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (_sock_fd < 0) {
        perror("socket");
        throw std::runtime_error("Failed to create SocketEngine::_sock_fd!");
    }
    db<SocketEngine>(INF) << "[SocketEngine] Raw socket created (fd=" << _sock_fd << ").\n";

    // 2. Making it non-blocking (important for use with NIC's epoll)
    int flags = fcntl(_sock_fd, F_GETFL, 0);
    if (flags == -1) {
         perror("fcntl F_GETFL");
         close(_sock_fd); _sock_fd = -1;
         throw std::runtime_error("Failed to get socket flags!");
    }
    if (fcntl(_sock_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL O_NONBLOCK");
        close(_sock_fd); _sock_fd = -1;
        throw std::runtime_error("Failed to set socket non-blocking!");
    }
    db<SocketEngine>(INF) << "[SocketEngine] Socket set to non-blocking.\n";

    // 3. Getting interface index
    struct ifreq ifr;
    std::memset(&ifr, 0, sizeof(ifr));
    std::strncpy(ifr.ifr_name, INTERFACE(), IFNAMSIZ - 1); // Safer copy
    ifr.ifr_name[IFNAMSIZ - 1] = '\0'; // Ensure null termination

    if (ioctl(_sock_fd, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl SIOCGIFINDEX");
        close(_sock_fd); _sock_fd = -1;
        throw std::runtime_error("Failed to retrieve interface index!");
    }
    _if_index = ifr.ifr_ifindex;
    db<SocketEngine>(INF) << "[SocketEngine] Interface index set: " << _if_index << " for " << INTERFACE() << "\n";

    // 4. Getting MAC address
    std::memset(&ifr, 0, sizeof(ifr)); // Clear ifr again
    std::strncpy(ifr.ifr_name, INTERFACE(), IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';

    if (ioctl(_sock_fd, SIOCGIFHWADDR, &ifr) < 0) {
        perror("ioctl SIOCGIFHWADDR");
        close(_sock_fd); _sock_fd = -1;
        throw std::runtime_error("Failed to retrieve MAC address!");
    }

    std::memcpy(_mac_address.bytes, ifr.ifr_hwaddr.sa_data, Ethernet::MAC_SIZE);
    db<SocketEngine>(INF) << "[SocketEngine] MAC address set: " << Ethernet::mac_to_string(_mac_address) << "\n";

    // 5. Bind socket to interface
    struct sockaddr_ll sll;
    std::memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_ALL);
    sll.sll_ifindex = _if_index;

    if (bind(_sock_fd, reinterpret_cast<struct sockaddr*>(&sll), sizeof(sll)) < 0) {
        perror("bind");
        close(_sock_fd); _sock_fd = -1;
        throw std::runtime_error("Failed to bind SocketEngine socket to interface!");
    }

    db<SocketEngine>(INF) << "[SocketEngine] Socket setup complete.";
}

// Destructor: Ensure socket is closed
SocketEngine::~SocketEngine()  {
    db<SocketEngine>(TRC) << "SocketEngine::~SocketEngine() called!\n";
    // stop(); // Stop should be called explicitly before destruction if needed
    if (_sock_fd >= 0) {
        close(_sock_fd);
        db<SocketEngine>(INF) << "[SocketEngine] Socket closed in destructor.\n";
    }
    // No epoll or eventfd to close
};

// Check running state
const bool SocketEngine::running() const {
    // Consider if _sock_fd >= 0 is a better check than just the atomic flag
    return _running.load(std::memory_order_acquire);
}

// Send: Remains largely the same
int SocketEngine::send(Ethernet::Frame* frame, unsigned int size) {
    db<SocketEngine>(TRC) << "SocketEngine::send() called!\n";

    if (!running()) {
        db<SocketEngine>(WRN) << "[SocketEngine] Attempted to send while engine is stopped.\n";
        return -1;
    }

     if (_sock_fd < 0) {
        db<SocketEngine>(ERR) << "[SocketEngine] Attempted to send with invalid socket FD.\n";
        return -1;
    }

    if (!frame || size < Ethernet::HEADER_SIZE) {
        db<SocketEngine>(ERR) << "[SocketEngine] Invalid frame or size for send().\n";
        return -1;
    }

    sockaddr_ll addr = {};
    addr.sll_family   = AF_PACKET;
    addr.sll_protocol = htons(frame->prot); // Protocol for addressing, not the frame field yet
    addr.sll_ifindex  = _if_index;
    addr.sll_halen    = Ethernet::MAC_SIZE;
    // Send to broadcast MAC at link layer, NIC/Protocol handles virtual dest
    std::memcpy(addr.sll_addr, Ethernet::BROADCAST.bytes, Ethernet::MAC_SIZE);

    // Make sure protocol field *in the frame* is in network byte order before sending
    // We store it in host order internally, convert just for sendto
    frame->prot = htons(frame->prot);

    int result = sendto(_sock_fd, frame, size, 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    int errno_saved = (result < 0) ? errno : 0;

    // Convert the protocol back to host order immediately after sending
    frame->prot = ntohs(frame->prot);

    if (result < 0) {
        errno = errno_saved; // Restore errno for perror
        perror("sendto");
        db<SocketEngine>(ERR) << "[SocketEngine] Failed to send frame: {src=" << Ethernet::mac_to_string(frame->src)
                             << ", dst=" << Ethernet::mac_to_string(frame->dst)
                             << ", prot=" << frame->prot // Log host order
                             << ", size=" << size << "}. errno=" << errno_saved << "\n";
    } else {
        db<SocketEngine>(INF) << "[SocketEngine] Frame sent: {src=" << Ethernet::mac_to_string(frame->src)
                             << ", dst=" << Ethernet::mac_to_string(frame->dst)
                             << ", prot=" << frame->prot // Log host order
                             << ", sent_size=" << result << "}\n";
    }

    return result;
}

// Receive: Called by NIC when socket FD is ready
int SocketEngine::receiveFrame(Ethernet::Frame& frame_buffer) {
    db<SocketEngine>(TRC) << "SocketEngine::receiveFrame() called!\n";

    if (!running()) {
        db<SocketEngine>(WRN) << "[SocketEngine] receiveFrame() called while engine stopping/stopped.\n";
        return -1; // Or a specific error code? TODO: Define error handling.
    }

    if (_sock_fd < 0) {
        db<SocketEngine>(ERR) << "[SocketEngine] receiveFrame() called with invalid socket FD.\n";
        return -1;
    }

    struct sockaddr_ll src_addr;
    socklen_t addr_len = sizeof(src_addr);

    // Receive into the provided frame_buffer reference
    int bytes_received = recvfrom(_sock_fd, &frame_buffer, sizeof(Ethernet::Frame), 0, // Read max frame size
                                  reinterpret_cast<sockaddr*>(&src_addr), &addr_len);

    if (bytes_received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            db<SocketEngine>(INF) << "[SocketEngine] No data available to receive (EAGAIN/EWOULDBLOCK).\n";
            return 0; // Indicate no data currently available, not an error
        } else {
            perror("recvfrom");
            db<SocketEngine>(ERR) << "[SocketEngine] recvfrom error. errno=" << errno << "\n";
            return -1; // Indicate a receive error
        }
    }

    // Check for valid Ethernet frame size (at least header size)
    if (static_cast<unsigned int>(bytes_received) < Ethernet::HEADER_SIZE) {
        db<SocketEngine>(ERR) << "[SocketEngine] Received undersized frame (" << bytes_received << " bytes), dropping.\n";
        return -2; // Indicate invalid frame received
    }

    // Convert protocol from network to host byte order *before* returning
    frame_buffer.prot = ntohs(frame_buffer.prot);

    db<SocketEngine>(INF) << "[SocketEngine] Received frame: {src=" << Ethernet::mac_to_string(frame_buffer.src)
                         << ", dst=" << Ethernet::mac_to_string(frame_buffer.dst)
                         << ", prot=" << frame_buffer.prot // Log host order
                         << ", size=" << bytes_received << "}\n";

    // Return the actual number of bytes received
    return bytes_received;
}

// --- Interface methods required by NIC ---

Ethernet::Address SocketEngine::getMacAddress() const {
     db<SocketEngine>(TRC) << "SocketEngine::getMacAddress() called.\n";
     return _mac_address;
}

int SocketEngine::getNotificationFd() const {
     db<SocketEngine>(TRC) << "SocketEngine::getNotificationFd() called.\n";
     // Return the raw socket FD for the NIC to monitor
     return _sock_fd;
}

// run() method removed

#endif // SOCKETENGINE_H