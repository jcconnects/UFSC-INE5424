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

// Assuming these are the correct includes for the concrete types
#include "protocol.h"
#include "nic.h"
#include "socketEngine.h" // Corrected filename
#include "communicator.h"
#include "message.h"
// #include "vehicle.h" // REMOVED to break circular dependency
#include "debug.h" // Include for db<> logging

// Forward declare Vehicle instead of including the full header
class Vehicle;

// --- Concrete Type Definitions ---
// TODO: Consider moving these to a central "types.h"
using TheProtocol = Protocol<NIC<SocketEngine>>;
using TheAddress = TheProtocol::Address;
using TheCommunicator = Communicator<TheProtocol>;
using TheMessage = Message<TheCommunicator::MAX_MESSAGE_SIZE>;
// --- End Concrete Type Definitions ---


class Component {
public:
    // Constructor uses concrete types
    Component(Vehicle* vehicle, const std::string& name, TheProtocol* protocol, TheAddress address);

    // Virtual destructor for proper cleanup of derived classes
    virtual ~Component() = default;

    // Start method - Creates and launches the component's thread
    virtual void start();

    // Stop method - Signals the thread to stop and joins it
    virtual void stop();

    // Pure virtual run method - must be implemented by derived classes with the component's main loop
    virtual void run() = 0;

    std::string name() const { return _name; }

    // Accessors
    bool running() const { return _running.load(std::memory_order_acquire); }
    const std::string& name() const { return _name; }
    Vehicle* vehicle() const { return _vehicle; };
    std::ofstream* log_file() { return &_log_file; };

    // Concrete send and receive methods using defined types
    int send(const TheAddress& destination, const void* data, unsigned int size);
    int receive(void* data, unsigned int max_size, TheAddress* source_address = nullptr); // Optional source addr

protected:
    // Helper function to be called by the pthread_create
    static void* thread_entry_point(void* arg);

    // Common members
    Vehicle* _vehicle;
    std::string _name;
    std::atomic<bool> _running;
    pthread_t _thread;

    // Type-safe communicator storage and protocol pointer
    std::unique_ptr<TheCommunicator> _communicator;
    TheProtocol* _protocol; // Pointer to Vehicle's protocol instance

    // CSV logging functionality
    std::ofstream _log_file;
    void open_log_file(const std::string& filename_prefix);
    void close_log_file();

private:
    // Prevent copying and assignment
    Component(const Component&) = delete;
    Component& operator=(const Component&) = delete;
};

// --- Implementation ---

// Constructor
Component::Component(Vehicle* vehicle, const std::string& name, TheProtocol* protocol, TheAddress address)
    : _vehicle(vehicle),
      _name(name),
      _running(false),
      _thread(0), // Initialize thread handle
      _protocol(protocol)
{
    if (!_protocol) {
        throw std::invalid_argument("Component requires a non-null protocol instance.");
    }
    if (!_vehicle) {
         throw std::invalid_argument("Component requires a non-null vehicle instance.");
    }
    // Create the communicator for this component instance
    _communicator = std::make_unique<TheCommunicator>(_protocol, address);
    db<Component>(INF) << "Component '" << _name << "' created with address " << address << "\n";
}

// Start method
void Component::start() {
    db<Component>(TRC) << "Component::start() called for " << name() << "\n";
    if (running()) {
        db<Component>(WRN) << "Component " << name() << " already running.\n";
        return;
    }
    _running.store(true, std::memory_order_release);
    int rc = pthread_create(&_thread, nullptr, Component::thread_entry_point, this);
    if (rc) {
        db<Component>(ERR) << "ERROR; return code from pthread_create() is " << rc << " for " << name() << "\n";
        _running.store(false);
        throw std::runtime_error("Failed to create component thread for " + name());
    }
     db<Component>(INF) << "Component " << name() << " thread created successfully.\n";
}

// Stop method
void Component::stop() {
    db<Component>(TRC) << "Component::stop() called for component " << _name << "\n";
    if (running()) {
        _running.store(false, std::memory_order_release);
        // Optionally: Signal the communicator to interrupt blocking calls if implemented
        // if (_communicator) {
        //     _communicator->interrupt();
        // }
        if (_thread != 0) {
            int join_rc = pthread_join(_thread, nullptr);
            if (join_rc == 0) {
                 db<Component>(INF) << "Component '" << _name << "' thread joined.\n";
            } else {
                 db<Component>(ERR) << "Component '" << _name << "' failed to join thread! Error code: " << join_rc << "\n";
                 // Consider logging errno or using strerror(join_rc) if applicable
            }
             _thread = 0; // Reset thread handle after join
        }
    } else {
         db<Component>(WRN) << "Component::stop() called but component " << _name << " was not running.\n";
    }
    close_log_file(); // Close log file after stopping
}

// Static thread entry point
void* Component::thread_entry_point(void* arg) {
    Component* self = static_cast<Component*>(arg);
    db<Component>(INF) << "Component '" << self->name() << "' thread starting execution.\n";
    try {
        self->run(); // Call the derived class's implementation
    } catch (const std::exception& e) {
        db<Component>(ERR) << "Component '" << self->name() << "' thread caught exception: " << e.what() << "\n";
    } catch (...) {
        db<Component>(ERR) << "Component '" << self->name() << "' thread caught unknown exception.\n";
    }
    db<Component>(INF) << "Component '" << self->name() << "' thread finished execution.\n";
    self->_running.store(false, std::memory_order_relaxed); // Ensure running is false on exit
    return nullptr;
}


// Send method
int Component::send(const TheAddress& destination, const void* data, unsigned int size) {
    if (size > TheCommunicator::MAX_MESSAGE_SIZE) {
        db<Component>(ERR) << name() << "::send error: message size (" << size << ") exceeds MAX_MESSAGE_SIZE (" << TheCommunicator::MAX_MESSAGE_SIZE << ").\n";
        return -1; // Indicate error
    }

    // Construct message directly with data
    TheMessage msg(data, size);
    // msg.set_data(data, size); // REMOVED: Use constructor instead

    // Assuming Communicator::send is modified to take destination
    if (_communicator->send(destination, &msg)) {
        // Use INF log level from debug.h
        db<Component>(INF) << name() << "::send successful (" << msg.size() << " bytes to " << destination << ").\n";
        return size; // Return bytes sent on success
    } else {
        db<Component>(ERR) << name() << "::send failed.\n";
        return -1; // Indicate error
    }
    // Added default return path for safety, though above logic should cover it.
    // return -1; // Can be removed if the if/else guarantees a return
}

// Receive method
int Component::receive(void* data, unsigned int max_size, TheAddress* source_address) {
     // Removed the initial running() check. The loop in the derived run() method should handle this.
     // The check after the blocking call remains important.
    if (!_communicator) {
        db<Component>(ERR) << name() << "::receive called but communicator is null!\n";
        return 0; // Or throw
    }

    TheMessage msg;
    db<Component>(TRC) << name() << "::receive waiting for message...\n";

    // Assuming Communicator::receive signature is:
    // bool receive(MessageType* message, AddressType* source = nullptr);
    if (!_communicator->receive(&msg, source_address)) {
        // Check if we stopped while waiting
        if (!running()) {
            db<Component>(INF) << name() << "::receive interrupted by stop().\n";
            return -1; // Indicate stopped
        }
        // Only log warning if the component thinks it should still be running
        db<Component>(WRN) << name() << "::receive failed or timed out.\n";
        return 0; // Indicate receive error/timeout
    }

    db<Component>(TRC) << name() << "::receive received message of size " << msg.size() << (source_address ? " from " + std::string(source_address->to_string()) : "") << "\n";


    if (msg.size() > max_size) {
        db<Component>(ERR) << name() << "::receive buffer too small (" << max_size << " bytes) for received message (" << msg.size() << " bytes).\n";
        // Data is lost here! Consider alternatives if partial data is useful.
        return 0; // Indicate error (buffer overflow)
    }

    std::memcpy(data, msg.data(), msg.size());
    // Use TRC level and fix DEB typo
    db<Component>(TRC) << name() << "::receive successfully processed message (" << msg.size() << " bytes copied).\n";
    return msg.size(); // Return bytes received
}

// Logging methods - updated to use prefix
void Component::open_log_file(const std::string& filename_prefix) {
    close_log_file(); // Ensure any previous file is closed
    std::string log_dir = "./logs"; // TODO: Make configurable?
    // Potential improvement: Create logs directory if it doesn't exist
    std::string filename = log_dir + "/" + filename_prefix + "_" + name() + ".csv";
    _log_file.open(filename);
    if (!_log_file.is_open()) {
         db<Component>(ERR) << name() << " failed to open log file: " << filename << "\n";
    } else {
         db<Component>(INF) << name() << " opened log file: " << filename << "\n";
         // Derived classes should write headers immediately after calling this in their constructor
    }
}

void Component::close_log_file() {
    if (_log_file.is_open()) {
         db<Component>(INF) << name() << " closing log file.\n";
        _log_file.close();
    }
}

#endif // COMPONENT_H 