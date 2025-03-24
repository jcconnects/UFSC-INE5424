#ifndef TRAITS_H
#define TRAITS_H

class SocketEngine;

// Traits definition
template <typename T>
struct Traits {};

template <>
struct Traits<NIC<SocketEngine>>
{
    static const unsigned int SEND_BUFFERS = 16;
    static const unsigned int RECEIVE_BUFFERS = 16;
};

template <>
struct Traits<Protocol<NIC<SocketEngine>>>
{
    static const unsigned short ETHERNET_PROTOCOL_NUMBER = 0x8000; // Example value
};

template<>
struct Traits<SocketEngine>
{
    static constexpr char* INTERFACE_NAME = "eth0";
};

#endif // TRAITS_H