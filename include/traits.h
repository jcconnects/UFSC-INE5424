#ifndef TRAITS_H
#define TRAITS_H

// Foward Declarations
template <typename NIC>
class Protocol;

template <typename Engine>
class NIC;

class SocketEngine;

class Debug;

template <typename Channel>
class Communicator;

class Vehicle;


// Traits definition
template <typename T>
struct Traits {
    static const bool debugged = false;
};

// Traits for NIC class
template <typename Engine>
struct Traits<NIC<Engine>> : public Traits<void>
{
    static const bool debugged = false;
    static const unsigned int SEND_BUFFERS = 16;
    static const unsigned int RECEIVE_BUFFERS = 16;
};

// Traits for Protocol class
template <typename NIC>
struct Traits<Protocol<NIC>> : public Traits<void>
{
    static const bool debugged = false;
};

// Traits for Protocol<NIC<SocketEngine>> class
template <>
struct Traits<Protocol<NIC<SocketEngine>>> : public Traits<void>
{
    static const bool debugged = false;
    static const unsigned int ETHERNET_PROTOCOL_NUMBER = 888; // Example value
};

// Traits for SocketEngine class
template<>
struct Traits<SocketEngine> : public Traits<void>
{
    static const bool debugged = false;
    static constexpr const char* INTERFACE_NAME = "eth0";
};

// Traits for Communicator class
template<typename Channel>
struct Traits<Communicator<Channel>> : public Traits<void>
{
    static const bool debugged = false;
};

// Traits for Vehicle class
template<>
struct Traits<Vehicle> : public Traits<void>
{
    static const bool debugged = true;
};

//traits para classe Debug
template<>
struct Traits<Debug> : public Traits<void>
{
    static const bool error = false;
    static const bool warning = false;
    static const bool info = true;
    static const bool trace = true;
};


#endif // TRAITS_H