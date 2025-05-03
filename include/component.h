#ifndef COMPONENT_H
#define COMPONENT_H

#include <atomic>
#include <string>
#include <pthread.h>
#include <fstream>
#include <memory> // For std::unique_ptr
#include <stdexcept> // For std::invalid_argument
#include <cstring> // For std::memcpy
#include <chrono> // Required by some derived components - TODO: Move time logic later?
#include <unistd.h> // Required by some derived components - TODO: Move sleep logic later?
#include <iostream> // For std::cerr
#include <vector>
#include <functional> // For std::function
#include <sstream> // For std::stringstream
#include <filesystem>

// Include necessary headers
#include "communicator.h"
#include "debug.h" // Include for db<> logging

// Forward declarations
class Vehicle;

template <typename Engine1, typename Engine2>
class NIC;

template <typename NIC>
class Protocol;

class SocketEngine;

class SharedMemoryEngine;


class Component {
    public:
        typedef NIC<SocketEngine, SharedMemoryEngine> VehicleNIC;
        typedef Protocol<VehicleNIC> VehicleProt;
        typedef Communicator<VehicleProt> Comms;
        typedef Comms::Address Address;
        typedef Comms::Message Message;

        // Constructor uses concrete types
        Component(Vehicle* vehicle, const unsigned int vehicle_id, const std::string& name);

        // Virtual destructor for proper cleanup of derived classes
        virtual ~Component();

        // Start method - Creates and launches the component's thread
        void start();

        // Stop method - Signals the thread to stop and joins it
        void stop();

        // Pure virtual run method - must be implemented by derived classes with the component's main loop
        virtual void run() = 0;

        // Getters
        const bool running() const;
        const std::string& getName() const;
        const Vehicle* vehicle() const;
        std::ofstream* log_file();
        const Address& address() const;

        // Concrete send and receive methods using defined types
        int send(const void* data, unsigned int size, Address destination = Address::BROADCAST);
        int receive(void* data, unsigned int max_size, Address* source_address = nullptr); // Optional source addr

    protected:
        // Helper function to be called by the pthread_create
        static void* thread_entry_point(void* arg);

        // Common members
        const Vehicle* _vehicle;
        std::string _name;
        std::atomic<bool> _running;
        pthread_t _thread;

        // Type-safe communicator storage
        Comms* _communicator;

        // CSV logging functionality
        std::string _filename;
        std::string _log_dir;
        std::ofstream _log_file;
        void open_log_file();
        void close_log_file();

    private:
        // Prevent copying and assignment
        Component(const Component&) = delete;
        Component& operator=(const Component&) = delete;
};


/************* Component Implementation ******************/
Component::Component(Vehicle* vehicle,  const unsigned int vehicle_id, const std::string& name) : _vehicle(vehicle), _name(name), _running(false), _thread(0) {
    if (!_vehicle) {
         throw std::invalid_argument("Component requires a non-null vehicle instance.");
    }

    // Communicator will be created in each specific component Constructor, due to port setting";

    // Setting log filename
    _filename = _name + std::to_string(vehicle_id) + "_log.csv";

    // Setting log dir
    _log_dir = "./logs/vehicle_" + std::to_string(vehicle_id) + "/"; 
}

Component::~Component() {
    delete _communicator;
}

void Component::start() {
    db<Component>(TRC) << "Component::start() called for compoenent " << getName() << ".\n";

    if (running()) {
        db<Component>(WRN) << "[Component] start() called when component" << getName() << " is already running.\n";
        return;
    }

    _running.store(true, std::memory_order_release);

    int rc = pthread_create(&_thread, nullptr, Component::thread_entry_point, this);
    if (rc) {
        db<Component>(ERR) << "[Component] return code from pthread_create() is " << rc << " for "<< getName() << "\n";
        _running.store(false);
        throw std::runtime_error("Failed to create component thread for " + getName());
    }
     db<Component>(INF) << "Component " << getName() << " thread created successfully.\n";
}

void Component::stop() {
    db<Component>(TRC) << "Component::stop() called for component " << getName() << "\n";

    if (!running()) {
        db<Component>(WRN) << "[Component] stop() called for component " << getName() << " when it is already stopped.\n";
        return;
    }

    // Fisrt set its own running state
    _running.store(false, std::memory_order_release);
    
    // Then stops communicator, releasing any blocked threads
    _communicator->close();
    db<Component>(INF) << "[Component] Communicator closed for component" << getName() << ".\n";
    
    // Then join thread
    if (_thread != 0) {
        int join_rc = pthread_join(_thread, nullptr);

        if (join_rc == 0) {
                db<Component>(INF) << "[Component] thread joined for component " << getName() << ".\n";
        } else {
                db<Component>(ERR) << "[Component] failed to join thread for component " << getName() << "! Error code: " << join_rc << "\n";
                // Consider logging errno or using strerror(join_rc) if applicable
        }

        _thread = 0; // Reset thread handle after join
    }

    close_log_file(); // Close log file after stopping
    db<Component>(INF) << "[Component] " << getName() <<" stopped.\n";
}

void* Component::thread_entry_point(void* arg) {
    Component* self = static_cast<Component*>(arg);

    db<Component>(INF) << "[Component] " << self->getName() << " thread starting execution.\n";

    try {
        self->run(); // Call the derived class's implementation
    } catch (const std::exception& e) {
        db<Component>(ERR) << "[Component] " << self->getName() << " thread caught exception: " << e.what() << "\n";
    } catch (...) {
        db<Component>(ERR) << "[Component] '" << self->getName() << " thread caught unknown exception.\n";
    }

    db<Component>(INF) << "[Component] " << self->getName() << " thread finished execution.\n";
    return nullptr;
}


int Component::send(const void* data, unsigned int size, Address destination) {
    
    // Construct message 
    Message* msg = _communicator->new_message(data, size);
    
    if (!msg) {
        db<Component>(ERR) << "[Component] " << getName() << " failed to create message\n";
        return 0;
    }

    // Assuming Communicator::send is modified to take destination
    if (_communicator->send(msg, destination)) {
        db<Component>(INF) << "[Component] " << getName() << " sent " << msg->size() << " bytes to " << destination.to_string() << ".\n";
        // Memory management
        delete msg;
        return size; // Return bytes sent on success
    } else {
        db<Component>(ERR) << "[Component] " << getName() << " failed to send message.\n";
        // Memory management
        delete msg;
        return 0; // Indicate error
    }
}

// Receive method
int Component::receive(void* data, unsigned int max_size, Address* source_address) {
    db<Component>(TRC) << "[Component] " << getName() << " receive called!\n";

    // Creating empty message
    Message msg;

    if (!_communicator->receive(&msg)) {
        // Check if we stopped while waiting (only for log purposes)
        if (!running()) {
            db<Component>(INF) << "[Component] " << getName() << " receive interrupted by stop().\n";
        }

        db<Component>(WRN) << "[Component] " << getName() << " receive failed.\n";
        return 0; // Indicate error
    }

    // Sets source address
    if (source_address)
        *source_address = msg.origin();

    db<Component>(TRC) << "[Component] " << getName() << " received message of size " << msg.size() << ".\n";

    if (msg.size() > max_size) {
        db<Component>(ERR) << "[Component] "<< getName() << " buffer too small (" << max_size << " bytes) for received message (" << msg.size() << " bytes).\n";
        // Data is lost here! Consider alternatives if partial data is useful.
        return 0; // Indicate error
    }

    // Copy message content to data pointer
    std::memcpy(data, msg.data(), msg.size());
    db<Component>(INF) << "[Component]" << getName() << " received data: " << std::string(static_cast<const char*>(data), msg.size()) << "\n";
    

    // Return bytes received
    return msg.size();
}

// Logging methods
void Component::open_log_file() {
    // Ensure any previous file is closed
    close_log_file(); 

    // Creating directory, in case it doesn't exists yet
    std::filesystem::create_directory(_log_dir);
    std::filesystem::permissions(_log_dir,  std::filesystem::perms::others_all);

    // Defines filepath
    std::string filepath = _log_dir + _filename;

    // Opens file
    _log_file.open(filepath);

    if (!_log_file.is_open()) {
        db<Component>(ERR) << "[Component] " << getName() << " failed to open log file: " << filepath << "\n";
    } else {
        db<Component>(INF) << "[Component] " << getName() << " opened log file: " << filepath << "\n";
    }
    
    // Derived classes should write headers immediately after calling this in their constructor
}

void Component::close_log_file() {
    if (_log_file.is_open()) {
        _log_file.close();
        db<Component>(INF) << "[Component] " << getName() << " log file closed.\n";
    }
}

const bool Component::running() const {
    return _running.load(std::memory_order_acquire);
}

const std::string& Component::getName() const {
    return _name;
}

const Vehicle* Component::vehicle() const {
    return _vehicle;
}

std::ofstream* Component::log_file() {
    return &_log_file;
}

const Component::Address& Component::address() const {
    return _communicator->address();
}
#endif // COMPONENT_H 