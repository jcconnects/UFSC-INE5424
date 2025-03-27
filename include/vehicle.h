#ifndef VEHICLE_H
#define VEHICLE_H

#include <string>
#include <chrono>
#include <thread>
#include "initializer.h"
#include "message.h"

// Vehicle class that uses the communication stack
class Vehicle {
    friend class Initializer;
public:
    typedef Initializer::VehicleConfig Config;
    
    // Modified constructor without Communicator
    template <typename N, typename P>
    Vehicle(const Config& config, N* nic, P* protocol)
        : _config(config), _is_communicator_set(false) {
        
        _nic = static_cast<void*>(nic);
        _protocol = static_cast<void*>(protocol);
        _communicator = nullptr;
        
        log("Vehicle created with NIC and Protocol");
        
        // Create the communicator
        createCommunicator(protocol);
    }
    
    ~Vehicle() {
        // In this simple implementation, we won't try to free memory
        log("Vehicle destroyed");
    }
    
    // Run the communication cycle
    void communicate() {
        log("Beginning communication cycle");
        
        if (!_is_communicator_set) {
            error("Communicator is not properly set up");
            return;
        }
        
        int counter = 0;
        while (counter++ < 10) {
            // Create message
            auto now = std::chrono::system_clock::now();
            auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count();
            
            std::string msgContent = "Vehicle " + std::to_string(_config.id) + 
                                    " message " + std::to_string(counter) + 
                                    " at " + std::to_string(time_ms);
            
            Message<std::string> msg(msgContent, msgContent.size());
            
            // Simulate sending
            log("Sending message: " + msg.data());
            
            // Small delay to simulate network
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            // Get new timestamp for received message
            now = std::chrono::system_clock::now();
            time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count();
            
            // Simulate receiving with timestamp
            log("Message received at " + std::to_string(time_ms) + " (simulated)");
            
            std::this_thread::sleep_for(std::chrono::milliseconds(_config.period_ms));
        }
        
        log("Communication complete");
    }
    
    // Log a message
    void log(const std::string& message) {
        if (_config.verbose_logging) {
            std::cout << _config.log_prefix << "[Vehicle " << _config.id << "] " 
                      << message << std::endl;
        }
    }

    void error(const std::string& message) {
        std::cerr << _config.log_prefix << "[Vehicle " << _config.id << "] ERROR: " 
                  << message << std::endl;
    }

private:
    // Private method to create communicator
    template <typename P>
    void createCommunicator(P* protocol) {
        log("Creating Communicator");
        
        // Create Protocol address
        auto address = typename P::Address(
            static_cast<typename P::Physical_Address>(
                static_cast<NIC<SocketEngine>*>(_nic)->address()
            ),
            static_cast<typename P::Port>(_config.id)
        );
        
        // Create Communicator and attach to Protocol
        _communicator = static_cast<void*>(
            new Communicator<P>(protocol, address)
        );
        
        _is_communicator_set = true;
        log("Communicator created successfully");
    }
    
    // Constructor is private, only Initializer can create
    Vehicle(const Config& config)
        : _config(config), _nic(nullptr), _protocol(nullptr), _communicator(nullptr), _is_communicator_set(false) {
        
        log("Vehicle created");
    }
    
    Config _config;
    
    // These will be initialized by the Initializer
    void* _nic;
    void* _protocol;
    void* _communicator;
    bool _is_communicator_set;
};

#endif // VEHICLE_H