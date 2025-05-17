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
template <typename GenericNIC>
class Protocol;

template <typename ExternalEngine, typename InternalEngine>
class NIC;

class SocketEngine;
class SharedMemoryEngine;

class Debug;

template <typename Channel>
class Communicator;

class Message;

class Vehicle;

class Component;

class BatteryComponent;

class CameraComponent;

class ECUComponent;

class GatewayComponent;

class INSComponent;

class LidarComponent;

class BasicProducer;

class BasicConsumer;

// Traits definition
template <typename T>
struct Traits {
    static const bool debugged = false;
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
};

// Traits for dual-engine NIC
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

// Traits for Communicator class
template<typename Channel>
struct Traits<Communicator<Channel>> : public Traits<void>
{
    static const bool debugged = true;
};

// Traits for message class
template <>
struct Traits<Message> : public Traits<void>
{
    static constexpr unsigned int MAC_SIZE = 16;
    static const bool debugged = false;
};

// Traits for Vehicle class
template<>
struct Traits<Vehicle> : public Traits<void>
{
    static const bool debugged = false;
};

// Traits for Component class
template <>
struct Traits<Component> : public Traits<void>
{
    static const bool debugged = false;
};

// Traits for BatteryComponent class
template <>
struct Traits<BatteryComponent>: public Traits<void>
{
    static const bool debugged = false;
};

// Traits for CameraComponent class
template <>
struct Traits<CameraComponent>: public Traits<void>
{
    static const bool debugged = false;
};

// Traits for ECUComponent class
template <>
struct Traits<ECUComponent>: public Traits<void>
{
    static const bool debugged = false;
};

template <>
struct Traits<GatewayComponent>: public Traits<void>
{
    static const bool debugged = true;
};

// Traits for INSComponent class
template <>
struct Traits<INSComponent> : public Traits<void>
{
    static const bool debugged = false;
};

// Traits for LidarComponent class
template <>
struct Traits<LidarComponent> : public Traits<void>
{
    static const bool debugged = false;
};

// Traits for BasicProducer class
template <>
struct Traits<BasicProducer> : public Traits<void>
{
    static const bool debugged = true;
};

// Traits for BasicConsumer class
template <>
struct Traits<BasicConsumer> : public Traits<void>
{
    static const bool debugged = false;
};

// Traits for Debug class
template<>
struct Traits<Debug> : public Traits<void>
{
    static const bool error = true;
    static const bool warning = true;
    static const bool info = true;
    static const bool trace = true;
};


#endif // TRAITS_H