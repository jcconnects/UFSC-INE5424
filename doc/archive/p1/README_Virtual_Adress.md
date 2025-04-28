# Changes resulting from the implementation virtual MAC address

## Overview
This document highlights the main changes made during the implementation of virtual MAC address, to distinguish vehicles.

## Key Changes

### SocketEngine
- Still sets real MAC addres, but the attribute name has been changed: from `SocketEngine::_address` to `SocketEngine::_mac_address`;
- On `SocketEngine::send()` method, changed socket addr definition, passing real MAC address as origin, regardless of the source address value passed in the frame.
```cpp
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
    
    (...)
}
```

### NIC
- Defines new attribute `Ethernet::Address NIC::_address`. This will be the virtual address.
- On the constructor, NIC sets `_address = this->_mac_address`, i.e, sets real address as NIC address. That allows the usage of the application interface with different real network interfaces
```cpp
template <typename Engine>
NIC<Engine>::NIC() {
    db<NIC>(TRC) << "NIC<Engine>::NIC() called!\n";

    for (unsigned int i = 0; i < N_BUFFERS; ++i) {
        _buffer[i] = DataBuffer();
        _free_buffers.push(&_buffer[i]);
    }
    db<NIC>(INF) << "[NIC] " << std::to_string(N_BUFFERS) << " buffers created\n";

    sem_init(&_buffer_sem, 0, N_BUFFERS);
    sem_init(&_binary_sem, 0, 1);

    // Setting default address
    _address = this->_mac_address;
}
```

### Initializer
- Before creating Protocol, on `Initializer::create_vehicle()`, sets NIC address (virtual) to a random MAC address, considering Vehicle id
```cpp
Vehicle* Initializer::create_vehicle(unsigned int id) {
    // Setting Vehicle virtual MAC Address
    Ethernet::Address addr;
    addr.bytes[0] = 0x02; // local, unicast
    addr.bytes[1] = 0x00;
    addr.bytes[2] = 0x00;
    addr.bytes[3] = 0x00;
    addr.bytes[4] = (id >> 8) & 0xFF;
    addr.bytes[5] = id & 0xFF;

    VehicleNIC* nic = new VehicleNIC();
    nic->setAddress(addr);
    CProtocol* protocol = new CProtocol(nic);
    return new Vehicle(id, nic, protocol);
}
```

## Why it works?
- `NIC::alloc()` sets `frame->src` as `NIC::address()`. That means that frame source address can be virtual or real, it doens't matter, since real MAC address will always be used on `SocketEngine::send()`, and frame is a raw data (void*), which does not affects socket's `sendto()` method.