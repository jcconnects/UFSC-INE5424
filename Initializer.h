// Initializer.h
#ifndef INITIALIZER_H
#define INITIALIZER_H

#include <memory>
#include <string>
#include <unistd.h>
#include <iostream>
//#include "engine.h"
//#include "message.h"

// Stub Message class (instead of including "message.h")
class Message {
public:
    Message(const std::string& content = "") : _content(content) {}
    
    const std::string& data() const { return _content; }
    size_t size() const { return _content.size(); }
    
private:
    std::string _content;
};

// Stub SocketEngine class (instead of including "engine.h")
class SocketEngine {
public:
    SocketEngine() {
        std::cout << "[Engine] Created SocketEngine" << std::endl;
    }
    ~SocketEngine() {
        std::cout << "[Engine] Destroyed SocketEngine" << std::endl;
    }
    
    std::string address() const { return "00:11:22:33:44:55"; }
};

// Stub NIC class
template <typename Engine>
class NIC {
public:
    typedef std::string Address;
    typedef int Protocol_Number;
    
    NIC() {
        std::cout << "[NIC] Created NIC" << std::endl;
    }
    
    ~NIC() {
        std::cout << "[NIC] Destroyed NIC" << std::endl;
    }
    
    std::string address() const { return "aa:bb:cc:dd:ee:ff"; }
    
    void attach(void* obs, Protocol_Number prot) {
        std::cout << "[NIC] Observer attached" << std::endl;
    }
    
    void detach(void* obs, Protocol_Number prot) {
        std::cout << "[NIC] Observer detached" << std::endl;
    }
};

// Stub Protocol class
template <typename N>
class Protocol {
public:
    static const typename N::Protocol_Number PROTO = 0x800;
    typedef typename N::Address Physical_Address;
    typedef int Port;
    
    class Address {
    public:
        Address() {}
        Address(Physical_Address paddr, Port port) {}
        static const Address BROADCAST;
    };
    
    Protocol(N* nic) : _nic(nic) {
        std::cout << "[Protocol] Created Protocol" << std::endl;
        _nic->attach(this, PROTO);
    }
    
    ~Protocol() {
        std::cout << "[Protocol] Destroyed Protocol" << std::endl;
        _nic->detach(this, PROTO);
    }
    
    bool send(const void* data, unsigned int size) {
        std::cout << "[Protocol] Sending message" << std::endl;
        return true;
    }
    
    bool receive(void* data, unsigned int size) {
        std::cout << "[Protocol] Receiving message" << std::endl;
        return true;
    }
    
    void attach(void* obs, Address address) {
        std::cout << "[Protocol] Observer attached" << std::endl;
    }
    
    void detach(void* obs, Address address) {
        std::cout << "[Protocol] Observer detached" << std::endl;
    }
    
private:
    N* _nic;
};

template <typename N>
const typename Protocol<N>::Address Protocol<N>::Address::BROADCAST;

// Stub Communicator class
template <typename C>
class Communicator {
public:
    typedef typename C::Address Address;
    
    Communicator(C* channel, Address address) : _channel(channel), _address(address) {
        std::cout << "[Communicator] Created Communicator" << std::endl;
        _channel->attach(this, address);
    }
    
    ~Communicator() {
        std::cout << "[Communicator] Destroyed Communicator" << std::endl;
        _channel->detach(this, _address);
    }
    
    bool send(const Message* message) {
        std::cout << "[Communicator] Sending message: " << message->data() << std::endl;
        return true;
    }
    
    bool receive(Message* message) {
        std::cout << "[Communicator] Receiving message" << std::endl;
        return true;
    }
    
private:
    C* _channel;
    Address _address;
};

// Forward declarations
template <typename E>
class NIC;
template <typename N>
class Protocol;
template <typename C>
class Communicator;
class Vehicle;

// Initializer class responsible for creating and managing a single vehicle process
class Initializer {
public:
    // Configuration for vehicle instance
    struct VehicleConfig {
        int id;
        int period_ms;
        bool verbose_logging;
        std::string log_prefix;
    };

    Initializer(const VehicleConfig& config);
    ~Initializer();

    // Start the vehicle process
    pid_t startVehicle();
    
    // Wait for the vehicle process to complete
    int waitForCompletion();
    
    // Terminate the vehicle process
    void terminateVehicle();
    
    // Check if vehicle is running
    bool isRunning() const;
    
    // Get process ID
    pid_t getPid() const;

private:
    // This is the method that runs inside the vehicle process
    void runVehicleProcess();
    
    // Create the communication stack
    template <typename Engine>
    void setupCommunicationStack();

    // Friend declarations
    friend class Vehicle;
    template <typename E> 
    friend class NIC;
    template <typename N> 
    friend class Protocol;
    
    VehicleConfig _config;
    pid_t _vehicle_pid;
    bool _running;
};

// Vehicle class that uses the communication stack
class Vehicle {
    friend class Initializer;
public:
    typedef Initializer::VehicleConfig Config;
    
    // Modified constructor without Communicator
    template <typename N, typename P>
    Vehicle(const Config& config, N* nic, P* protocol);
    
    ~Vehicle();
    
    // Run the communication cycle
    void communicate();
    
    // Log a message
    void log(const std::string& message);
    void error(const std::string& message);

private:
    // Private method to create communicator
    template <typename P>
    void createCommunicator(P* protocol);
    
    // Constructor is private, only Initializer can create
    Vehicle(const Config& config);
    
    Config _config;
    
    // These will be initialized by the Initializer
    void* _nic;
    void* _protocol;
    void* _communicator;
    bool _is_communicator_set;
};

#endif // INITIALIZER_H