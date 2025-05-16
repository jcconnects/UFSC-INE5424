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
#include <sys/stat.h> // For mkdir
#include <linux/types.h>
#include <linux/sched.h>
#include <time.h>
#include <sys/syscall.h>

// Remove namespace alias to fix linter errors
// namespace fs = std::filesystem;
#include "communicator.h" // Includes message.h implicitly
#include "debug.h" // Include for db<> logging
#include "teds.h" // Added for DataTypeId
#include "observed.h" // Added for Conditionally_Data_Observed
#include "TypedDataHandler.h" // Include for TypedDataHandler
#include "ethernet.h" // For Ethernet::BROADCAST

// Forward declaration for GatewayComponent - removed direct include
class GatewayComponent;

// Forward declarations
class Vehicle;
class TypedDataHandler; // Added Forward Declaration

// Make Communicator a friend class to access component state for filtering
template <typename Channel>
class Communicator;

template <typename Engine1, typename Engine2>
class NIC;

template <typename NIC>
class Protocol;

class SocketEngine;

class SharedMemoryEngine;

// Define syscall numbers for SCHED_DEADLINE if not already defined
#ifndef SCHED_DEADLINE
#define SCHED_DEADLINE 6
#endif

#ifndef __NR_sched_setattr
#ifdef __x86_64__
#define __NR_sched_setattr 314
#define __NR_sched_getattr 315
#elif __i386__
#define __NR_sched_setattr 351
#define __NR_sched_getattr 352
#elif __arm__
#define __NR_sched_setattr 380
#define __NR_sched_getattr 381
#elif __aarch64__
#define __NR_sched_setattr 274
#define __NR_sched_getattr 275
#endif
#endif

// Define struct for sched_attr if not already defined
struct sched_attr {
    __u32 size;
    __u32 sched_policy;
    __u64 sched_flags;
    __s32 sched_nice;
    __u32 sched_priority;
    __u64 sched_runtime;
    __u64 sched_deadline;
    __u64 sched_period;
};

// Define syscall functions for sched_deadlinedirectory
static inline int sched_setattr(pid_t pid, const struct sched_attr *attr, unsigned int flags) {
    return syscall(__NR_sched_setattr, pid, attr, flags);
}

static inline int sched_getattr(pid_t pid, struct sched_attr *attr, unsigned int size, unsigned int flags) {
    return syscall(__NR_sched_getattr, pid, attr, size, flags);
}

class Component {
    public:
        typedef NIC<SocketEngine, SharedMemoryEngine> VehicleNIC;
        typedef Protocol<VehicleNIC> VehicleProt;
        typedef Communicator<VehicleProt> Comms;
        typedef Comms::Address Address;
        // Message class is now directly accessible via #include "message.h" (through communicator.h)

        // Define GatewayComponent::PORT constant to resolve dependencies - made public
        static const unsigned int GATEWAY_PORT = 0;

        // Constructor uses concrete types
        Component(Vehicle* vehicle, const unsigned int vehicle_id, const std::string& name);

        // Virtual destructor for proper cleanup of derived classes
        virtual ~Component();

        // Start method - Creates and launches the component's thread(s)
        void start();

        // Stop method - Signals the thread(s) to stop and joins it
        void stop();

        // Pure virtual run method - must be implemented by derived classes with the component's main loop
        // This can be re-evaluated for P3 components; might not be the primary execution path for all.
        virtual void run() = 0;

        // Getters
        const bool running() const;
        const std::string& getName() const;
        const Vehicle* vehicle() const;
        std::ofstream* log_file();
        const Address& address() const;
        
        // Get component role for filtering
        ComponentType type() const { return determine_component_type(); }

        // Concrete send and receive methods using defined types
        // These will be updated/augmented by P3 specific send/receive patterns for Interest/Response
        int send(const void* data, unsigned int size, Address destination = Address::BROADCAST);
        int receive(Message* msg); // Optional source addr

        // --- P3 Consumer Method - Public API for registering interest in a DataTypeId ---
        void register_interest_handler(DataTypeId type, std::uint32_t period_us, std::function<void(const Message&)> callback);

        // P3 Producer/Consumer accessors for Communicator filtering
        DataTypeId get_produced_data_type() const { return _produced_data_type; }
        bool has_active_interests() const { return !_active_interests.empty(); }
        
        // Friend declaration for Communicator to access Component's private members for filtering
        template <typename Channel>
        friend class Communicator;

        // Add helper method to check for SCHED_DEADLINE capability
        bool has_deadline_scheduling_capability();

    protected:
        // Helper function to be called by the pthread_create for the main run() thread
        static void* thread_entry_point(void* arg);
        
        // Helper method to determine component type
        ComponentType determine_component_type() const;
        void set_address(const Address& addr) { _address = addr; }

        // Common members
        const Vehicle* _vehicle;
        std::string _name;
        std::atomic<bool> _running; // Overall component operational status
        pthread_t _thread; // Original main thread for run()

        // Type-safe communicator storage
        Comms* _communicator;
        Address _gateway_address; // Address of the gateway component
        Address _address;

        // CSV logging functionality
        std::string _filename;
        std::string _log_dir;
        std::ofstream _log_file;
        void open_log_file();
        void close_log_file();
        std::string initialize_log_directory(unsigned int vehicle_id);

        // --- P3 Core Members ---
        Conditionally_Data_Observed<Message, DataTypeId> _internal_typed_observed;
        pthread_t _component_dispatcher_thread_id {0};
        std::atomic<bool> _dispatcher_running {false};

        // --- P3 Consumer-Specific Members ---
        std::vector<std::unique_ptr<TypedDataHandler>> _typed_data_handlers;
        std::vector<pthread_t> _handler_threads_ids;
        struct InterestRequest {
            DataTypeId type;
            std::uint32_t period_us;
            std::uint64_t last_accepted_response_time_us = 0;
            bool interest_sent = false;
            std::function<void(const Message&)> callback; // Callback associated with this interest
        };
        std::vector<InterestRequest> _active_interests;

        // --- P3 Producer-Specific Members ---
        DataTypeId _produced_data_type = DataTypeId::UNKNOWN; // Derived producer classes will set this
        std::vector<std::uint32_t> _received_interest_periods;
        std::atomic<std::uint32_t> _current_gcd_period_us {0};
        pthread_t _producer_response_thread_id {0};
        std::atomic<bool> _producer_thread_running {false};
        std::atomic<bool> _has_dl_capability {false}; // Store SCHED_DEADLINE capability state

        // --- P3 Dispatcher Methods ---
        static void* component_dispatcher_launcher(void* context);
        virtual void component_dispatcher_routine();

        // --- P3 Producer Methods ---
        static void* producer_response_launcher(void* context);
        void producer_response_routine();
        void start_producer_response_thread();
        void stop_producer_response_thread();
        std::uint32_t update_gcd_period();
        static std::uint32_t calculate_gcd(std::uint32_t a, std::uint32_t b);
        void store_interest_period(std::uint32_t period);
        
        // Virtual method for generating response data - Producers will override
        virtual bool produce_data_for_response(DataTypeId type, std::vector<std::uint8_t>& out_value) { return false; }

        // Virtual hook for when a producer's registration is confirmed (e.g., ack from Gateway)
        virtual void on_producer_registration_confirmed() {
            db<Component>(INF) << "[Component] [" << _address.to_string() << "] " << getName() << " Producer response mechanism activated.\n";
        }
};

#include "vehicle.h" // Now that all dependencies are resolved, include vehicle.h for implementation

// Component implementation
Component::Component(Vehicle* vehicle, const unsigned int vehicle_id, const std::string& name) 
    : _vehicle(vehicle), _name(name), _running(false)
{
    db<Component>(TRC) << "[Component] " << name << " Constructor called for component " << "\n";
    
    // Initialize log file directory
    _log_dir = initialize_log_directory(vehicle_id);
    _filename = _log_dir + "/" + _name + ".csv";
    
    // Set up the Gateway's address using the vehicle address and PORT 0
    _gateway_address = Address(_vehicle->address(), GATEWAY_PORT); // Gateway uses PORT 0
    
    db<Component>(INF) << "[Component] " << name << " created with Gateway at " 
                      << _gateway_address.to_string() << "\n";
}

Component::~Component() {
    db<Component>(TRC) << "[Component] [" << _address.to_string() << "] Destructor called for component " << _name << "\n";
    
    // Ensure component is stopped before destruction - may already have been called
    if (running()) {
        stop();
    }
    
    // Close log file if open
    close_log_file();
    
    // Clean up communicator if it exists
    if (_communicator) {
        delete _communicator;
        _communicator = nullptr;
    }
    
    db<Component>(INF) << "[Component] [" << _address.to_string() << "] Component " << _name << " destroyed\n";
}

// Entry point for component thread
void* Component::thread_entry_point(void* arg) {
    Component* component = static_cast<Component*>(arg);
    if (component) {
        try {
            db<Component>(TRC) << "[Component] [" << component->_address.to_string() << "] Thread entry point for " << component->getName() << "\n";
            component->run();
        } catch (const std::exception& e) {
            db<Component>(ERR) << "[Component] [" << component->_address.to_string() << "] " << component->getName() 
                              << " thread exception: " << e.what() << "\n";
        }
    }
    return nullptr;
}

// Start method - creates the component's thread
void Component::start() {
    db<Component>(TRC) << "[Component] [" << _address.to_string() << "] start() called for " << _name << "\n";
    if (running()) {
        db<Component>(WRN) << "[Component] [" << _address.to_string() << "] " << _name << " already running\n";
        return;
    }
    
    // Start main thread
    _running.store(true, std::memory_order_release);
    int result = pthread_create(&_thread, nullptr, thread_entry_point, this);
    if (result != 0) {
        _running.store(false, std::memory_order_release);
        throw std::runtime_error(std::string("Failed to create thread for component ") + _name);
    }
    
    // Launch the dispatcher thread
    _dispatcher_running.store(true, std::memory_order_release);
    result = pthread_create(&_component_dispatcher_thread_id, nullptr, component_dispatcher_launcher, this);
    if (result != 0) {
        _dispatcher_running.store(false, std::memory_order_release);
        db<Component>(ERR) << "[Component] [" << _address.to_string() << "] " << _name << " Failed to create dispatcher thread\n";
    }
    
    // For producer components, start the response thread
    if (_produced_data_type != DataTypeId::UNKNOWN) {
        start_producer_response_thread();
    }
    
    db<Component>(INF) << "[Component] [" << _address.to_string() << "] " << _name << " started\n";
}

// Stop method - signals the thread to stop and joins it
void Component::stop() {
    db<Component>(TRC) << "[Component] [" << _address.to_string() << "] stop() called for " << _name << "\n";
    if (!running()) {
        db<Component>(WRN) << "[Component] [" << _address.to_string() << "] " << _name << " already stopped\n";
        return;
    }
    
    // First, stop all interest handler processing threads
    for (auto& handler : _typed_data_handlers) {
        handler->stop_processing_thread();
    }
    
    // Clean up handler threads
    for (auto& thread_id : _handler_threads_ids) {
        pthread_join(thread_id, nullptr);
    }
    _handler_threads_ids.clear();
    
    // Set dispatcher running flag to false
    _dispatcher_running.store(false, std::memory_order_release);
    
    // Close communicator to unblock any threads waiting in receive()
    if (_communicator) {
        _communicator->close();
        db<Component>(TRC) << "[Component] [" << _address.to_string() << "] " << _name << " communicator closed\n";
    }
    
    // Now join dispatcher thread
    if (_component_dispatcher_thread_id != 0) {
        pthread_join(_component_dispatcher_thread_id, nullptr);
        _component_dispatcher_thread_id = 0;
    }
    
    // Stop producer thread if active
    if (_producer_thread_running.load(std::memory_order_acquire)) {
        stop_producer_response_thread();
    }
    
    // Stop main thread
    _running.store(false, std::memory_order_release);
    pthread_join(_thread, nullptr);
    
    db<Component>(INF) << "[Component] [" << _address.to_string() << "] " << _name << " stopped\n";
}

// Helper method to determine component type
ComponentType Component::determine_component_type() const {
    // Determine component type from class type or configuration
    // This is a simplified example - in reality, use RTTI or type hints
    if (_name.find("Gateway") != std::string::npos) {
        return ComponentType::GATEWAY;
    }
    else if (_name.find("Producer") != std::string::npos || _produced_data_type != DataTypeId::UNKNOWN) {
        return ComponentType::PRODUCER;
    }
    else if (_name.find("Consumer") != std::string::npos || !_active_interests.empty()) {
        return ComponentType::CONSUMER;
    }
    else {
        // Determine type based on the component's behavior
        // Default to UNKNOWN if type cannot be determined
        return ComponentType::UNKNOWN;
    }
}

// send method implementation
int Component::send(const void* data, unsigned int size, Address destination) {
    if (!_communicator) {
        db<Component>(ERR) << "[Component] [" << _address.to_string() << "] " << _name << " send() failed: communicator not initialized\n";
        return -1;
    }
    
    if (!data || size == 0) {
        db<Component>(ERR) << "[Component] [" << _address.to_string() << "] " << _name << " send() failed: invalid data or size\n";
        return -1;
    }
    
    db<Component>(TRC) << "[Component] [" << _address.to_string() << "] " << _name << " sending " << size 
                      << " bytes to " << destination.to_string() << "\n";
    
    // Create a message object for sending
    Message message = _communicator->new_message(Message::Type::RESPONSE, DataTypeId::UNKNOWN, 0, data, size);
    
    // Send the message
    if (_communicator->send(message, destination)) {
        return size;
    }
    return -1;
}

// receive method implementation
int Component::receive(Message* msg) {
    if (!_communicator) {
        db<Component>(ERR) << "[Component] [" << _address.to_string() << "] " << _name << " receive() failed: communicator not initialized\n";
        return -1;
    }
    
    if (!msg) {
        db<Component>(ERR) << "[Component] [" << _address.to_string() << "] " << _name << " receive() failed: invalid message pointer\n";
        return -1;
    }
    
    // Call receive without timeout since it's not supported
    if (_communicator->receive(msg)) {
        db<Component>(TRC) << "[Component] [" << _address.to_string() << "] " << _name << " received message from " 
                          << msg->origin().to_string() << "\n";
        return msg->value_size(); // Use public value_size() method
    }
    
    return -1;
}

// Open the CSV log file for output
void Component::open_log_file() {
    try {
        _log_file.open(_filename); // Open in append mode
        if (!_log_file.is_open()) {
            db<Component>(ERR) << "[Component] [" << _name << "] Failed to open log file: " << _filename << "\n";
        } else {
            db<Component>(INF) << "[Component] [" << _name << "] Opened log file: " << _filename << "\n";
        }
    } catch (const std::exception& e) {
        db<Component>(ERR) << "[Component] [" << _name << "] Exception opening log file: " << e.what() << "\n";
    }
}

// Close the CSV log file
void Component::close_log_file() {
    if (_log_file.is_open()) {
        _log_file.close();
        db<Component>(INF) << "[Component] [" << _address.to_string() << "] " << _name << " Closed log file\n";
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
    if (!_log_file.is_open()) {
        open_log_file();
    }
    return &_log_file;
}

const Component::Address& Component::address() const {
    if (!_communicator) {
        // No communicator, but need to return something - return default address
        static const Address default_addr;
        return default_addr;
    }
    return _communicator->address();
}

// Create log directory structure
std::string Component::initialize_log_directory(unsigned int vehicle_id) {
    // Try in priority order: Docker logs dir, tests/logs dir, current dir
    std::string base_dir = "tests/logs";
    
    // Create vehicle-specific directory
    std::string vehicle_dir = base_dir + "/vehicle_" + std::to_string(vehicle_id);
    
    struct stat info;
    if (stat(vehicle_dir.c_str(), &info) != 0) {
        // Directory doesn't exist, try to create it
        if (mkdir(vehicle_dir.c_str(), 0777) != 0) {
            db<Component>(ERR) << "[Component] " << _name << " Failed to create directory: " << vehicle_dir << "\n";
            return base_dir; // Fallback to base dir
        }
    }
    
    // Create component directory for complex components that need multiple log files
    std::string component_dir = vehicle_dir + "/" + _name;
    if (stat(component_dir.c_str(), &info) != 0) {
        if (mkdir(component_dir.c_str(), 0777) != 0) {
            db<Component>(WRN) << "[Component] " << _name << " Failed to create component directory: " << component_dir << "\n";
            return vehicle_dir; // Fallback to vehicle dir
        }
    }
    
    db<Component>(INF) << "[Component] [" << _name << "] Using log directory: " << vehicle_dir << "\n";
    return vehicle_dir;
}

// P3 API: Register interest in a data type with a callback for handling responses
void Component::register_interest_handler(DataTypeId type, std::uint32_t period_us, 
                                         std::function<void(const Message&)> callback) {
    if (type == DataTypeId::UNKNOWN) {
        db<Component>(ERR) << "[Component] [" << _address.to_string() << "] " << _name << " Cannot register interest in UNKNOWN data type\n";
        return;
    }
    
    // Check if we already have an interest for this type
    for (auto& interest : _active_interests) {
        if (interest.type == type) {
            db<Component>(WRN) << "[Component] [" << _address.to_string() << "] " << _name << " Already registered interest in data type " 
                              << static_cast<int>(type) << ", updating period and callback\n";
            interest.period_us = period_us;
            interest.callback = callback;
            return;
        }
    }
    
    // Add to active interests
    InterestRequest request{type, period_us, 0, false, callback};
    _active_interests.push_back(request);
    
    // Create and send INTEREST message to Gateway
    // Use a temporary variable to store the period, since we can't modify the message directly
    Message interest_message = _communicator->new_message(Message::Type::INTEREST, type, period_us);
    
    // Send to Gateway component
    Address broadcast_addr(Ethernet::BROADCAST, 0); // Port 0 is broadcast
    _communicator->send(interest_message, broadcast_addr);
    
    db<Component>(INF) << "[Component] [" << _address.to_string() << "] " << _name << " Sent INTEREST for data type " 
                      << static_cast<int>(type) << " with period " << period_us << "us\n";
}

// Entry point for component dispatcher thread
void* Component::component_dispatcher_launcher(void* context) {
    Component* component = static_cast<Component*>(context);
    if (component) {
        try {
            db<Component>(TRC) << "[Component] [" << component->_address.to_string() << "] Dispatcher thread starting for " << component->getName() << "\n";
            component->component_dispatcher_routine();
        } catch (const std::exception& e) {
            db<Component>(ERR) << "[Component] [" << component->_address.to_string() << "] " << component->getName() 
                              << " dispatcher thread exception: " << e.what() << "\n";
        }
    }
    return nullptr;
}

// Default implementation of component_dispatcher_routine
void Component::component_dispatcher_routine() {
    db<Component>(TRC) << "[Component] [" << _address.to_string() << "] " << _name << " dispatcher routine started\n";
    
    // This is a generic implementation that all components use to listen for messages
    // In the P3 architecture, producers receive INTERESTs, consumers receive RESPONSEs
    while (_dispatcher_running.load()) {
        // Create a message to hold received data
        Message message = _communicator->new_message(Message::Type::INTEREST, DataTypeId::UNKNOWN);
        
        // Blocking receive without timeout (API doesn't support timeout)
        if (_communicator->receive(&message)) {
            // Handle based on message type
            Message::Type msg_type = message.message_type();
            DataTypeId data_type = message.unit_type();
            
            if (msg_type == Message::Type::INTEREST && _produced_data_type != DataTypeId::UNKNOWN) {
                // INTEREST message for a producer
                if (data_type == _produced_data_type) {
                    std::uint32_t period = message.period();
                    
                    db<Component>(INF) << "[Component] [" << _address.to_string() << "] " << _name << " received INTEREST for data type " 
                                      << static_cast<int>(data_type) << " with period " << period << "us\n";
                    
                    // Store the interest period
                    store_interest_period(period);

                    // Start producer response thread if it's not already running
                    if (!_producer_thread_running.load(std::memory_order_acquire)) {
                        db<Component>(INF) << "[Component] [" << _address.to_string() << "] " << _name << " received first INTEREST, starting producer response thread.\n";
                        start_producer_response_thread();
                    }

                } else {
                    db<Component>(TRC) << "[Component] [" << _address.to_string() << "] " << _name << " ignoring INTEREST for unproduced data type " 
                                      << static_cast<int>(data_type) << "\n";
                }
            } 
            else if (msg_type == Message::Type::RESPONSE) {
                // RESPONSE message for a consumer
                // Check if we have an active interest in this data type
                bool interest_found = false;
                for (auto& interest : _active_interests) {
                    if (interest.type == data_type) {
                        interest_found = true;
                        
                        // Mark that we've received a response to our interest
                        interest.interest_sent = true;
                        
                        // Update last accepted response time
                        interest.last_accepted_response_time_us = 
                            std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                        
                        // Call the callback function if provided
                        if (interest.callback) {
                            interest.callback(message);
                        }
                        
                        db<Component>(TRC) << "[Component] [" << _address.to_string() << "] " << _name << " dispatched RESPONSE for data type " 
                                          << static_cast<int>(data_type) << " to callback\n";
                        break;
                    }
                }
                
                if (!interest_found) {
                    db<Component>(TRC) << "[Component] [" << _address.to_string() << "] " << _name << " received RESPONSE for data type " 
                                      << static_cast<int>(data_type) << " but no active interest\n";
                }
            }
            else {
                db<Component>(TRC) << "[Component] [" << _address.to_string() << "] " << _name << " received unhandled message type: " 
                                  << static_cast<int>(msg_type) << "\n";
            }
        } else {
            // Small sleep to avoid tight loop on error
            usleep(1000);  // 1ms sleep
        }
    }
    
    db<Component>(TRC) << "[Component] [" << _address.to_string() << "] " << _name << " dispatcher routine exiting\n";
}

// Entry point for producer response thread
void* Component::producer_response_launcher(void* context) {
    Component* component = static_cast<Component*>(context);
    if (component) {
        try {
            db<Component>(TRC) << "[Component] [" << component->_address.to_string() << "] Producer response thread starting for " << component->getName() << "\n";
            component->producer_response_routine();
        } catch (const std::exception& e) {
            db<Component>(ERR) << "[Component] [" << component->_address.to_string() << "] " << component->getName() 
                              << " producer response thread exception: " << e.what() << "\n";
        }
    }
    return nullptr;
}

// Producer response routine - generates periodic responses based on interests
void Component::producer_response_routine() {
    db<Component>(TRC) << "[Component] [" << _address.to_string() << "] " << _name << " producer response routine started\n";
    
    struct timespec next_period;
    clock_gettime(CLOCK_MONOTONIC, &next_period);
    
    // Determine if SCHED_DEADLINE is available
    _has_dl_capability.store(has_deadline_scheduling_capability(), std::memory_order_relaxed);
    
    // Use SCHED_DEADLINE if available, otherwise SCHED_FIFO
    if (_has_dl_capability.load(std::memory_order_relaxed)) {
        db<Component>(INF) << "[Component] [" << _address.to_string() << "] " << _name << " using SCHED_DEADLINE for response generation\n";
        
        // Setup SCHED_DEADLINE parameters - based on current_gcd_period
        struct sched_attr attr_dl;
        memset(&attr_dl, 0, sizeof(attr_dl));
        attr_dl.size = sizeof(attr_dl);
        attr_dl.sched_policy = SCHED_DEADLINE;
        attr_dl.sched_flags = 0;
        
        while (_producer_thread_running.load(std::memory_order_acquire)) {
            // Get the current GCD period
            std::uint32_t current_period = _current_gcd_period_us.load(std::memory_order_acquire);
            
            // If we have no interests yet, sleep and wait
            if (current_period == 0 || _received_interest_periods.empty()) {
                usleep(100000); // Sleep for 100ms
                continue;
            }
            
            // Update SCHED_DEADLINE parameters based on current period
            attr_dl.sched_runtime = current_period * 500; // 50% of period in ns
            attr_dl.sched_deadline = current_period * 1000; // Period in ns
            attr_dl.sched_period = current_period * 1000; // Period in ns
            
            // Set scheduling parameters - may fail if not root/CAP_SYS_NICE
            int result = sched_setattr(0, &attr_dl, 0);
            if (result < 0) {
                db<Component>(WRN) << "[Component] [" << _address.to_string() << "] " << _name 
                                  << " failed to set SCHED_DEADLINE, falling back to SCHED_FIFO\n";
                _has_dl_capability.store(false, std::memory_order_relaxed);
                break; // Break and fall back to SCHED_FIFO
            }
            
            // Generate response based on the current period
            std::vector<std::uint8_t> response_data;
            if (produce_data_for_response(_produced_data_type, response_data)) {
                // Create response message with the data
                Message response = _communicator->new_message(
                    Message::Type::RESPONSE, 
                    _produced_data_type,
                    0,  // period is 0 for responses
                    response_data.data(),  // Pass the data buffer
                    response_data.size()   // and its size
                );
                
                // Send to Gateway for broadcast distribution
                Address broadcast_addr(Ethernet::BROADCAST, 0); // Port 0 is broadcast
                _communicator->send(response, broadcast_addr);
                
                db<Component>(TRC) << "[Component] [" << _address.to_string() << "] " << _name << " sent RESPONSE for data type " 
                                  << static_cast<int>(_produced_data_type) << " with " 
                                  << response_data.size() << " bytes\n";
            }
            
            // Sleep for one period
            clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_period, NULL);
            
            // Calculate next period
            next_period.tv_nsec += current_period * 1000;
            if (next_period.tv_nsec >= 1000000000) {
                next_period.tv_sec += next_period.tv_nsec / 1000000000;
                next_period.tv_nsec %= 1000000000;
            }
        }
    }
    else {
        db<Component>(INF) << "[Component] [" << _address.to_string() << "] " << _name << " using SCHED_FIFO for response generation\n";
        
        // Setup SCHED_FIFO
        struct sched_param param;
        param.sched_priority = 99; // Max RT priority
        pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
        
        // Response generation loop
        while (_producer_thread_running.load(std::memory_order_acquire)) {
            // Get the current GCD period
            std::uint32_t current_period = _current_gcd_period_us.load(std::memory_order_acquire);
            
            // If we have no interests yet, sleep and wait
            if (current_period == 0 || _received_interest_periods.empty()) {
                usleep(100000); // Sleep for 100ms
                continue;
            }
            
            // Generate response based on the current period
            std::vector<std::uint8_t> response_data;
            if (produce_data_for_response(_produced_data_type, response_data)) {
                // Create response message with the data
                Message response = _communicator->new_message(
                    Message::Type::RESPONSE, 
                    _produced_data_type,
                    0,  // period is 0 for responses
                    response_data.data(),  // Pass the data buffer
                    response_data.size()   // and its size
                );
                
                // Send to Gateway for broadcast distribution
                Address broadcast_addr(Ethernet::BROADCAST, 0); // Port 0 is broadcast
                _communicator->send(response, broadcast_addr);
                
                db<Component>(TRC) << "[Component] [" << _address.to_string() << "] " << _name << " sent RESPONSE for data type " 
                                  << static_cast<int>(_produced_data_type) << " with " 
                                  << response_data.size() << " bytes\n";
            }
            
            // Sleep for one period using usleep
            usleep(current_period);
        }
    }
    
    db<Component>(TRC) << "[Component] [" << _address.to_string() << "] " << _name << " producer response routine exiting\n";
}

// Start the producer response thread
void Component::start_producer_response_thread() {
    if (_produced_data_type == DataTypeId::UNKNOWN) {
        db<Component>(WRN) << "[Component] [" << _address.to_string() << "] " << _name << " cannot start producer thread, no data type produced\n";
        return;
    }
    
    if (_producer_thread_running.load(std::memory_order_acquire)) {
        db<Component>(WRN) << "[Component] [" << _address.to_string() << "] " << _name << " producer thread already running\n";
        return;
    }
    
    // Start the producer thread
    _producer_thread_running.store(true, std::memory_order_release);
    
    int result = pthread_create(&_producer_response_thread_id, nullptr, producer_response_launcher, this);
    if (result != 0) {
        _producer_thread_running.store(false, std::memory_order_release);
        db<Component>(ERR) << "[Component] [" << _address.to_string() << "] " << _name << " failed to create producer response thread\n";
        return;
    }
    
    db<Component>(INF) << "[Component] [" << _address.to_string() << "] " << _name << " started producer response thread\n";
    
    // Call virtual hook for when registration is confirmed (derived classes can override)
    on_producer_registration_confirmed();
}

// Stop the producer response thread
void Component::stop_producer_response_thread() {
    if (!_producer_thread_running.load(std::memory_order_acquire)) {
        db<Component>(WRN) << "[Component] [" << _address.to_string() << "] " << _name << " producer thread not running\n";
        return;
    }
    
    // Signal the thread to stop
    _producer_thread_running.store(false, std::memory_order_release);
    
    // Join the thread
    if (_producer_response_thread_id != 0) {
        pthread_join(_producer_response_thread_id, nullptr);
        _producer_response_thread_id = 0;
    }
    
    db<Component>(INF) << "[Component] [" << _address.to_string() << "] " << _name << " stopped producer response thread\n";
}

// Calculate the greatest common divisor (GCD) of all interest periods
std::uint32_t Component::update_gcd_period() {
    if (_received_interest_periods.empty()) {
        return 0;
    }
    
    if (_received_interest_periods.size() == 1) {
        return _received_interest_periods[0];
    }
    
    // Calculate GCD of all periods
    std::uint32_t result = _received_interest_periods[0];
    for (size_t i = 1; i < _received_interest_periods.size(); i++) {
        result = calculate_gcd(result, _received_interest_periods[i]);
    }
    
    return result;
}

// Helper function to calculate GCD using Euclidean algorithm
std::uint32_t Component::calculate_gcd(std::uint32_t a, std::uint32_t b) {
    while (b != 0) {
        std::uint32_t temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

// Method to store and handle a new interest period
void Component::store_interest_period(std::uint32_t period) {
    bool found = false;
    for (auto& p : _received_interest_periods) {
        if (p == period) {
            found = true;
            break;
        }
    }
    
    if (!found) {
        _received_interest_periods.push_back(period);
        _current_gcd_period_us.store(update_gcd_period(), std::memory_order_release);
        db<Component>(INF) << "[Component] [" << _address.to_string() << "] " << _name << " updated GCD period to " 
                          << _current_gcd_period_us.load() << "us\n";
    }
}

// Check if SCHED_DEADLINE is available on this system
bool Component::has_deadline_scheduling_capability() {
    // Try to set SCHED_DEADLINE with a test struct
    struct sched_attr attr_test;
    memset(&attr_test, 0, sizeof(attr_test));
    attr_test.size = sizeof(attr_test);
    attr_test.sched_policy = SCHED_DEADLINE;
    attr_test.sched_runtime = 10000000;  // 10ms
    attr_test.sched_deadline = 100000000; // 100ms
    attr_test.sched_period = 100000000;  // 100ms

    // Try to set deadline scheduling - requires CAP_SYS_NICE (e.g., running as root)
    int result = sched_setattr(0, &attr_test, 0);
    
    if (result == 0) {
        // Success, set back to SCHED_OTHER
        struct sched_param param;
        param.sched_priority = 0;
        pthread_setschedparam(pthread_self(), SCHED_OTHER, &param);
        return true;
    }
    
    return false;
}

#endif // COMPONENT_H 