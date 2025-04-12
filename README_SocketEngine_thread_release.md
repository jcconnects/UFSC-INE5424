# Changes resulting from the implementation of SocketEngine with blocking epoll

## Overview
This document highlights the main changes made during the implementation of SocketEngine with blocking epoll, to optimize latency.

## General Changes
- Added extra debug messages on SocketEngine, NIC, Protocol and Communicator classes.

## Key Changes

### Observer x Observed classes
- Reinstate inheritance of Concurrent_Observed from Conditionally_Data_Observed. The class is still unused, but this reaffirms the design structure.

### Protocol
- With the changes in Observer pattern, Protocol class is using Conditionally_Data_Observed again.

### SocketEngine
- Removed timeout from epoll_wait. Now, thread stays sleeping until an event occurs.
- To release the thread, created an stop event, defined by `SocketEngine::_stop_ev`;
```cpp
void SocketEngine::setUpEpoll() {
    db<SocketEngine>(TRC) << "SocketEngine::setUpEpoll() called!\n";

    (...)

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
```
- On NIC destructor, it calls Engine::stop, that, now, sets `SocketEngine::_running` as false and triggers the stop event. Then waits for receive thread to finish.
```cpp
void SocketEngine::stop() {
    db<SocketEngine>(TRC) << "SocketEngine::run() called!\n";
    
    if (!_running) return;

    _running = false;

    std::uint64_t u = 1;
    write(_stop_ev, &u, sizeof(u));

    pthread_join(_receive_thread, nullptr);
    db<SocketEngine>(INF) << "[SocketEngine] sucessfully stopped!\n";
}
```

## Extra changes
- Setted `SocketEngine::handleSignal` as virtual method, that is, now, defined by `NIC<Engine>` class.
```cpp
template <typename Engine>
void NIC<Engine>::handleSignal() {
    db<SocketEngine>(TRC) << "SocketEngine::handleSignal() called!\n";
    
    Ethernet::Frame frame;
    struct sockaddr_ll src_addr;
    socklen_t addr_len = sizeof(src_addr);
    
    int bytes_received = recvfrom(this->_sock_fd, &frame, sizeof(frame), 0, reinterpret_cast<sockaddr*>(&src_addr), &addr_len);
                               
    if (bytes_received < 0) {
        db<SocketEngine>(INF) << "[SocketEngine] No data received\n";
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("recvfrom");
        }
        return;
    }
    
    // Check for valid Ethernet frame size (at least header size)
    if (static_cast<unsigned int>(bytes_received) < Ethernet::HEADER_SIZE) {
        db<SocketEngine>(ERR) << "[SocketEngine] Received undersized frame (" << bytes_received << " bytes)\n";
        return;
    }
    
    // Convert protocol from network to host byte order
    frame.prot = ntohs(frame.prot);
    db<SocketEngine>(INF) << "[SocketEngine] received frame: {src = " << Ethernet::mac_to_string(frame.src) << ", dst = " << Ethernet::mac_to_string(frame.dst) << ", prot = " << frame.prot << ", size = " << bytes_received << "}\n";
    
    // Process the frame if callback is set
    // 1. Extracting header
    Ethernet::Address dst = frame.dst;
    Ethernet::Protocol proto = frame.prot;

    // 2. Allocate buffer
    DataBuffer * buf = alloc(dst, proto, bytes_received);
    if (!buf) return;

    // 3. Copy frame to buffer
    std::memcpy(buf->data(), &frame, bytes_received);

    // 4. Notify Observers
    if (!notify(proto, buf)) {
        db<NIC>(INF) << "[NIC] data received, but no one was notified " << proto << "\n";
        free(buf); // if no one is listening, free buffer
    }
}
```
- Also removed all references to callbackMethod, by both NIC and SocketEngine