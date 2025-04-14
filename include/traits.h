#ifndef TRAITS_H
#define TRAITS_H

#include <string>
#include <fstream>

// Helper function to get the interface name
inline std::string get_interface_name() {
    std::string interface_name = "test-dummy0"; // Default
    std::ifstream iface_file("tests/logs/current_test_iface");
    if (iface_file) {
        std::getline(iface_file, interface_name);
        if (!interface_name.empty()) {
            return interface_name;
        }
    }
    return interface_name;
}

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

class Component;

// Traits definition
template <typename T>
struct Traits {
    static const bool debugged = true;
};

// Traits for NIC class
template <typename Engine>
struct Traits<NIC<Engine>> : public Traits<void>
{
    static const bool debugged = true;
    static const unsigned int SEND_BUFFERS = 512;
    static const unsigned int RECEIVE_BUFFERS = 512;
};

// Traits for Protocol class
template <typename NIC>
struct Traits<Protocol<NIC>> : public Traits<void>
{
    static const bool debugged = true;
};

// Traits for Protocol<NIC<SocketEngine>> class
template <>
struct Traits<Protocol<NIC<SocketEngine>>> : public Traits<void>
{
    static const bool debugged = true;
    static const unsigned int ETHERNET_PROTOCOL_NUMBER = 888; // Example value
};

// Traits for SocketEngine class
template<>
struct Traits<SocketEngine> : public Traits<void>
{
    static const bool debugged = true;
    static constexpr const char* DEFAULT_INTERFACE_NAME = "test-dummy0";
    
    static const char* INTERFACE_NAME() {
        static std::string interface_name = get_interface_name();
        return interface_name.c_str();
    }
};

// Traits for Communicator class
template<typename Channel>
struct Traits<Communicator<Channel>> : public Traits<void>
{
    static const bool debugged = true;
};

// Traits for Vehicle class
template<>
struct Traits<Vehicle> : public Traits<void>
{
    static const bool debugged = true;
};

template <>
struct Traits<Component> : public Traits<void>
{
    static const bool debugged = true;
};

//traits para classe Debug
template<>
struct Traits<Debug> : public Traits<void>
{
    static const bool error = true;
    static const bool warning = true;
    static const bool info = true;
    static const bool trace = true;
};


#endif // TRAITS_H