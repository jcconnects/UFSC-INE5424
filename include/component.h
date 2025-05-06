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
        std::string initialize_log_directory(unsigned int vehicle_id);

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
    _log_dir = initialize_log_directory(vehicle_id);
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
    
    // Create response message with the data
    Message msg = _communicator->new_message(
        Message::Type::RESPONSE,  // Using RESPONSE type for raw data
        0,                        // Default type
        0,                        // No period for responses
        data,                     // The data to send
        size                      // Size of the data
    );
    
    // Send the message
    if (_communicator->send(msg, destination)) {
        db<Component>(INF) << "[Component] " << getName() << " sent " << size << " bytes to " << destination.to_string() << ".\n";
        return size; // Return bytes sent on success
    } else {
        db<Component>(ERR) << "[Component] " << getName() << " failed to send message.\n";
        return 0; // Indicate error
    }
}

// Receive method
int Component::receive(void* data, unsigned int max_size, Address* source_address) {
    db<Component>(TRC) << "[Component] " << getName() << " receive called!\n";

    // Creating a message for receiving
    Message msg = _communicator->new_message(Message::Type::RESPONSE, 0);

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

    // For RESPONSE messages, check the value
    if (msg.message_type() == Message::Type::RESPONSE) {
        const std::uint8_t* value_data = msg.value();
        unsigned int value_size = msg.size(); // This isn't quite right, but we need to estimate

        if (value_size > max_size) {
            db<Component>(ERR) << "[Component] "<< getName() << " buffer too small (" << max_size << " bytes) for received message (" << value_size << " bytes).\n";
            return 0; // Indicate error
        }

        if (value_data) {
            // Copy message value to data pointer
            std::memcpy(data, value_data, value_size);
            db<Component>(INF) << "[Component]" << getName() << " received data as RESPONSE\n";
            return value_size;
        }
    } 
    
    // For any other message type or if no value data
    const void* msg_data = msg.data();
    unsigned int msg_size = msg.size();
    
    if (msg_size > max_size) {
        db<Component>(ERR) << "[Component] "<< getName() << " buffer too small (" << max_size << " bytes) for received message (" << msg_size << " bytes).\n";
        // Data is lost here! Consider alternatives if partial data is useful.
        return 0; // Indicate error
    }

    // Copy raw message data
    std::memcpy(data, msg_data, msg_size);
    db<Component>(INF) << "[Component]" << getName() << " received raw message data\n";

    // Return bytes received
    return msg_size;
}

// Logging methods
void Component::open_log_file() {
    // Ensure any previous file is closed
    close_log_file(); 

    try {
        std::string filepath = _log_dir + _filename;
        _log_file.open(filepath);

        if (!_log_file.is_open()) {
            db<Component>(ERR) << "[Component] " << getName() << " failed to open log file: " << filepath << "\n";
            db<Component>(ERR) << "[Component] " << getName() << " will log to standard output instead.\n";
        } else {
            db<Component>(INF) << "[Component] " << getName() << " opened log file: " << filepath << "\n";
        }
    } catch (const std::exception& e) {
        db<Component>(ERR) << "[Component] " << getName() << " error opening log file: " << e.what() << "\n";
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

// Helper method to initialize log directory
std::string Component::initialize_log_directory(unsigned int vehicle_id) {
    std::string log_dir;
    
    // First try the Docker container's logs directory
    if (std::filesystem::exists("/app/logs")) {
        log_dir = "/app/logs/vehicle_" + std::to_string(vehicle_id) + "/";
    } else {
        // Try to use tests/logs directory instead of current directory
        if (!std::filesystem::exists("tests/logs")) {
            try {
                std::filesystem::create_directories("tests/logs");
            } catch (...) {
                // Ignore errors, will fall back if needed
            }
        }
        
        if (std::filesystem::exists("tests/logs")) {
            log_dir = "tests/logs/vehicle_" + std::to_string(vehicle_id) + "/";
        } else {
            // Last resort fallback to current directory
            log_dir = "./";
        }
    }
    
    // Create the directory if it doesn't exist
    if (log_dir != "./") {
        try {
            std::filesystem::create_directories(log_dir);
        } catch (...) {
            // If we can't create the directory, fall back to current directory
            log_dir = "./";
        }
    }
    
    return log_dir;
}
#endif // COMPONENT_H 