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

template <typename ExternalEngine, typename InternalEngine>
class NIC;

class SocketEngine;
class SharedMemoryEngine;

class Debug;

template <typename Channel>
class Communicator;

class Vehicle;

class Component;

// Traits definition
template <typename T>
struct Traits {
    static const bool debugged = false;
};

// New traits for dual-engine NIC
template <typename ExternalEngine, typename InternalEngine>
struct Traits<NIC<ExternalEngine, InternalEngine>> : public Traits<void>
{
    static const bool debugged = false;
    static const unsigned int SEND_BUFFERS = 512;
    static const unsigned int RECEIVE_BUFFERS = 512;
};

// Traits for Protocol with dual-engine NIC
template <>
struct Traits<Protocol<NIC<SocketEngine, SharedMemoryEngine>>> : public Traits<void>
{
    static const bool debugged = false;
    static const unsigned int ETHERNET_PROTOCOL_NUMBER = 888; // Example value
};

// Traits for SocketEngine class
template<>
struct Traits<SocketEngine> : public Traits<void>
{
    static const bool debugged = false;
    static constexpr const char* DEFAULT_INTERFACE_NAME = "test-dummy0";
    
    static const char* INTERFACE_NAME() {
        static std::string interface_name = get_interface_name();
        return interface_name.c_str();
    }
};

// Traits for SharedMemoryEngine class
template<>
struct Traits<SharedMemoryEngine> : public Traits<void>
{
    static const bool debugged = false;
    static const unsigned int BUFFER_SIZE = 128;     // Capacity of the shared memory ring buffer
    static const unsigned int POLL_INTERVAL_MS = 10; // Interval (ms) for the timerfd notification
    static const unsigned int MTU = 1500;            // Max payload size in shared frames (aligned with Ethernet)
};

// Define the static const members outside the class
const unsigned int Traits<SharedMemoryEngine>::BUFFER_SIZE;
const unsigned int Traits<SharedMemoryEngine>::POLL_INTERVAL_MS;
const unsigned int Traits<SharedMemoryEngine>::MTU;

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
    static const bool debugged = false;
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