#ifndef COMPONENT_H
#define COMPONENT_H

#include <atomic>
#include <string>
#include <pthread.h>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <cstring>
#include <chrono>
#include <unistd.h>
#include <vector>
#include <functional>
#include <mutex>
#include <algorithm>
#include <time.h>
#include <sys/stat.h>
#include <linux/types.h>
#include <sys/syscall.h>
#include <signal.h>  // For signal handling

// Add back the SCHED_DEADLINE support
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

// Signal handler for thread interruption
// Use a single static handler for the entire component system
extern "C" void component_signal_handler(int sig) {
    // Simply wake up the thread to check its running state
    if (sig == SIGUSR1) {
        // No action needed, just unblock from system calls
    }
}

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

// Define syscall functions for scheduling
static inline int sched_setattr(pid_t pid, const struct sched_attr *attr, unsigned int flags) {
    return syscall(__NR_sched_setattr, pid, attr, flags);
}

static inline int sched_getattr(pid_t pid, struct sched_attr *attr, unsigned int size, unsigned int flags) {
    return syscall(__NR_sched_getattr, pid, attr, size, flags);
}

// Remove complex thread management that's no longer needed
#include "communicator.h"
#include "debug.h"
#include "teds.h"
#include "ethernet.h"

// Forward declarations
class Vehicle;
class GatewayComponent;

// Make Communicator a friend class to access component state for filtering
template <typename Channel>
class Communicator;

template <typename Engine1, typename Engine2>
class NIC;

template <typename NIC>
class Protocol;

class SocketEngine;
class SharedMemoryEngine;

// Enum ComponentType might need to be defined here or in a common header
// Assuming communicator.h (which defines it) is included before or by component.h consumers
// For P3, ComponentType is defined in communicator.h which is included above.

class Component {
    public:
        typedef NIC<SocketEngine, SharedMemoryEngine> VehicleNIC;
        typedef Protocol<VehicleNIC> VehicleProt;
        typedef Communicator<VehicleProt> Comms;
        typedef Comms::Address Address;

        // Define constants for P3 ports
        inline static const unsigned int GATEWAY_PORT = 0;
        inline static const unsigned int INTERNAL_BROADCAST_PORT = 1;
        inline static const unsigned int MIN_COMPONENT_PORT = 2;

        // Constructor
        Component(Vehicle* vehicle, const unsigned int vehicle_id, const std::string& name, ComponentType component_type);

        // Virtual destructor
        virtual ~Component();

        // Start method - Creates and launches the component's thread
        void start();

        // Stop method - Signals the thread to stop and joins it
        void stop();

        // Pure virtual run method - must be implemented by derived classes
        virtual void run() = 0;

        // Getters
        const bool running() const;
        const std::string& getName() const;
        const Vehicle* vehicle() const;
        std::ofstream* log_file();
        const Address& address() const;
        
        // Get component role for filtering
        ComponentType type() const;

        // Basic send/receive methods
        int send(const void* data, unsigned int size, Address destination = Address::BROADCAST);
        int receive(Message* msg);

        // --- P3 Consumer Method - Register interest in a DataTypeId ---
        void register_interest(DataTypeId type, std::uint32_t period_us, std::function<void(const Message&)> callback);
        void send_interest_message();

        // P3 Producer/Consumer accessors for Communicator filtering
        DataTypeId get_produced_data_type() const { return _produced_data_type; }
        DataTypeId get_interest_type() const;
        std::uint32_t get_interest_period() const;
        
        // Friend declaration for Communicator
        template <typename Channel>
        friend class Communicator;

        // Make handle_interest_period publicly accessible for callback registration
        // Now accepts the full Message object for detailed logging
        void handle_interest_period(const Message& interest_msg) {
            std::lock_guard<std::mutex> lock(_periods_mutex);

            db<Component>(TRC) << "[Component] [" << _address.to_string() << "] handle_interest_period() called for interest from " 
                              << interest_msg.origin().to_string() << " with period " << interest_msg.period() << "\n";
            
            // Log INTEREST_RECEIVED event to CSV
            if (_log_file.is_open() && type() == ComponentType::PRODUCER) { // Ensure it's a producer logging this
                auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                _log_file << now_us << ","                                 // timestamp_us
                         << "PRODUCER" << ","                             // event_category
                         << "INTEREST_RECEIVED" << ","                 // event_type
                         << interest_msg.timestamp() << ","             // message_id (original msg timestamp)
                         << static_cast<int>(Message::Type::INTEREST) << ","// message_type
                         << static_cast<int>(interest_msg.unit_type()) << "," // data_type_id
                         << interest_msg.origin().to_string() << ","       // origin_address
                         << address().to_string() << ","                   // destination_address (Component's own)
                         << interest_msg.period() << ","                 // period_us
                         << "0" << ","                                  // value_size
                         << "-"                                         // notes
                         << "\n";
                _log_file.flush();
            }

            // Check if period already exists
            auto it = std::find(_interest_periods.begin(), _interest_periods.end(), interest_msg.period());
            if (it == _interest_periods.end()) {
                // Add new period
                _interest_periods.push_back(interest_msg.period());
                
                // Update GCD
                _current_gcd_period_us.store(update_gcd_period(), std::memory_order_release);
                
                db<Component>(INF) << "[Component] [" << _address.to_string() << "] " << _name << " updated GCD period to " 
                                  << _current_gcd_period_us.load() << "us after interest from " << interest_msg.origin().to_string() << "\n";
            }
        }

    protected:
        // Thread entry point
        static void* thread_entry_point(void* arg);
        
        // Update address
        void set_address(const Address& addr) { _address = addr; }

        // Common members
        const Vehicle* _vehicle;
        std::string _name;
        std::atomic<bool> _running;
        pthread_t _thread;

        // Type-safe communicator storage
        Comms* _communicator;
        Address _gateway_address;
        Address _address;

        // CSV logging
        std::string _filename;
        std::string _log_dir;
        std::ofstream _log_file;
        void open_log_file();
        void close_log_file();
        std::string initialize_log_directory(unsigned int vehicle_id);

        // --- P3 Simple Implementation Members ---
        
        // For producers
        DataTypeId _produced_data_type = DataTypeId::UNKNOWN;
        std::vector<std::uint32_t> _interest_periods;
        std::mutex _periods_mutex;
        std::atomic<std::uint32_t> _current_gcd_period_us {0};
        pthread_t _producer_thread {0};
        std::atomic<bool> _producer_thread_running {false};
        std::atomic<bool> _has_dl_capability {false}; // Flag for deadline scheduling capability
        
        // For consumers
        DataTypeId _interested_data_type = DataTypeId::UNKNOWN;
        std::uint32_t _interested_period_us = 0;
        std::function<void(const Message&)> _data_callback;

        // --- P3 Producer Methods ---
        static void* producer_thread_launcher(void* context);
        void producer_routine();
        void start_producer_thread();
        void stop_producer_thread();
        std::uint32_t update_gcd_period();
        static std::uint32_t calculate_gcd(std::uint32_t a, std::uint32_t b);

        // Check for deadline scheduling capability
        bool has_deadline_scheduling_capability();
        
        // Virtual method for generating response data - Producers will override
        virtual bool produce_data_for_response(DataTypeId type, std::vector<std::uint8_t>& out_value) { return false; }

    private:
        ComponentType _component_actual_type; // Store the actual type
};

#include "vehicle.h"

// Component implementation
Component::Component(Vehicle* vehicle, const unsigned int vehicle_id, const std::string& name, ComponentType component_type) 
    : _vehicle(vehicle), _name(name), _running(false), _component_actual_type(component_type)
{
    db<Component>(TRC) << "[Component] " << name << " Constructor called for component " << "\n";
    
    // Initialize log file directory
    _log_dir = initialize_log_directory(vehicle_id);
    _filename = _log_dir + "/" + _name + ".csv";
    
    // Set up the Gateway's address using the vehicle address and PORT 0
    _gateway_address = Address(_vehicle->address(), GATEWAY_PORT);
    
    db<Component>(INF) << "[Component] " << name << " created with Gateway at " 
                      << _gateway_address.to_string() << "\n";

    // Setup the handle_interest_period callback if this is a producer component
    if (component_type == ComponentType::PRODUCER) {
        // We need to register a callback to the Communicator to handle interest periods later
        // This is registered after the _communicator is created in the derived Component constructor
    }
}

Component::~Component() {
    db<Component>(TRC) << "[Component] [" << _address.to_string() << "] Destructor called for component " << _name << "\n";
    
    // Ensure component is stopped before destruction
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
    
    // For producer components, start the response thread
    if (_produced_data_type != DataTypeId::UNKNOWN) {
        start_producer_thread();
    }
    
    // For consumer components, send interest message if configured
    if (_interested_data_type != DataTypeId::UNKNOWN) {
        send_interest_message();
    }
    
    db<Component>(INF) << "[Component] [" << _address.to_string() << "] " << _name << " started\n";
}

void Component::stop() {
    db<Component>(TRC) << "[Component] [" << _address.to_string() << "] stop() called for " << _name << "\n";
    if (!running()) {
        db<Component>(WRN) << "[Component] [" << _address.to_string() << "] " << _name << " already stopped\n";
        return;
    }
    
    // Stop producer thread if active
    if (_producer_thread_running.load(std::memory_order_acquire)) {
        stop_producer_thread();
    }
    
    // Close communicator to unblock any threads waiting in receive()
    if (_communicator) {
        _communicator->close();
        db<Component>(TRC) << "[Component] [" << _address.to_string() << "] " << _name << " communicator closed\n";
    }
    
    // Stop main thread
    _running.store(false, std::memory_order_release);
    pthread_join(_thread, nullptr);
    
    db<Component>(INF) << "[Component] [" << _address.to_string() << "] " << _name << " stopped\n";
}

ComponentType Component::type() const {
    return _component_actual_type;
}

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

int Component::receive(Message* msg) {
    if (!_communicator) {
        db<Component>(ERR) << "[Component] [" << _address.to_string() << "] " << _name << " receive() failed: communicator not initialized\n";
        return -1;
    }
    
    if (_communicator->receive(msg)) {
        db<Component>(TRC) << "[Component] [" << _address.to_string() << "] " << _name << " received message from " 
                          << msg->origin().to_string() << "\n";
        return msg->value_size();
    }
    
    return -1;
}

void Component::open_log_file() {
    try {
        _log_file.open(_filename);
        if (!_log_file.is_open()) {
            db<Component>(ERR) << "[Component] [" << _name << "] Failed to open log file: " << _filename << "\n";
        } else {
            db<Component>(INF) << "[Component] [" << _name << "] Opened log file: " << _filename << "\n";
        }
    } catch (const std::exception& e) {
        db<Component>(ERR) << "[Component] [" << _name << "] Exception opening log file: " << e.what() << "\n";
    }
}

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
        static const Address default_addr;
        return default_addr;
    }
    return _communicator->address();
}

std::string Component::initialize_log_directory(unsigned int vehicle_id) {
    std::string base_dir = "tests/logs";
    std::string vehicle_dir = base_dir + "/vehicle_" + std::to_string(vehicle_id);
    
    struct stat info;
    if (stat(vehicle_dir.c_str(), &info) != 0) {
        if (mkdir(vehicle_dir.c_str(), 0777) != 0) {
            db<Component>(ERR) << "[Component] " << _name << " Failed to create directory: " << vehicle_dir << "\n";
            return base_dir;
        }
    }
    
    db<Component>(INF) << "[Component] [" << _name << "] Using log directory: " << vehicle_dir << "\n";
    return vehicle_dir;
}

void Component::register_interest(DataTypeId type, std::uint32_t period_us, std::function<void(const Message&)> callback) {
    if (type == DataTypeId::UNKNOWN) {
        db<Component>(ERR) << "[Component] [" << _address.to_string() << "] " << _name << " Cannot register interest in UNKNOWN data type\n";
        return;
    }
    
    // Store interest information
    _interested_data_type = type;
    _interested_period_us = period_us;
    _data_callback = callback;
    
    // Configure communicator to filter by this type
    if (_communicator) {
        _communicator->set_interest(type, period_us);
    }
    
    // Send interest message if component is running
    // This call will also handle logging INTEREST_SENT
    if (running()) {
        send_interest_message();
    }
    
    db<Component>(INF) << "[Component] [" << _address.to_string() << "] " << _name << " Registered interest in data type " 
                      << static_cast<int>(type) << " with period " << period_us << "us\n";
}

// Helper to send INTEREST message
void Component::send_interest_message() {
    if (_interested_data_type == DataTypeId::UNKNOWN || !_communicator) {
        return;
    }
    
    // Create INTEREST message
    Message interest_msg = _communicator->new_message(
        Message::Type::INTEREST, 
        _interested_data_type, 
        _interested_period_us
    );
    
    // Send to gateway port
    Address gateway_addr(Ethernet::BROADCAST, GATEWAY_PORT);
    if (_communicator->send(interest_msg, gateway_addr)) {
        db<Component>(INF) << "[Component] [" << _address.to_string() << "] " << _name 
                         << " Sent INTEREST for type " << static_cast<int>(_interested_data_type) 
                         << " with period " << _interested_period_us << "us\n";
        
        // Log INTEREST_SENT event to CSV (primarily for Consumers)
        if (_log_file.is_open() && type() == ComponentType::CONSUMER) { // Ensure it's a consumer logging this
            auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()).count();
            _log_file << now_us << ","                                 // timestamp_us
                     << "CONSUMER" << ","                             // event_category
                     << "INTEREST_SENT" << ","                     // event_type
                     << interest_msg.timestamp() << ","             // message_id
                     << static_cast<int>(Message::Type::INTEREST) << ","// message_type
                     << static_cast<int>(interest_msg.unit_type()) << "," // data_type_id
                     << address().to_string() << ","                   // origin_address (Component's own)
                     << gateway_addr.to_string() << ","              // destination_address (Gateway)
                     << interest_msg.period() << ","                 // period_us
                     << "0" << ","                                  // value_size
                     << "-"                                         // notes
                     << "\n";
            _log_file.flush();
        }
    }
}

// Get interest type for filtering
DataTypeId Component::get_interest_type() const {
    return _interested_data_type;
}

// Get interest period for filtering
std::uint32_t Component::get_interest_period() const {
    return _interested_period_us;
}

// Check for deadline scheduling capability
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

// Start the producer response thread
void Component::start_producer_thread() {
    if (_produced_data_type == DataTypeId::UNKNOWN) {
        db<Component>(WRN) << "[Component] [" << _address.to_string() << "] " << _name << " cannot start producer thread, no data type produced\n";
        return;
    }
    
    if (_producer_thread_running.load(std::memory_order_acquire)) {
        db<Component>(WRN) << "[Component] [" << _address.to_string() << "] " << _name << " producer thread already running\n";
        return;
    }
    
    // Install the signal handler for thread interruption
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = component_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, nullptr);
    
    // Check if deadline scheduling is available
    _has_dl_capability.store(has_deadline_scheduling_capability(), std::memory_order_relaxed);
    if (_has_dl_capability.load()) {
        db<Component>(INF) << "[Component] [" << _address.to_string() << "] " << _name << " will use SCHED_DEADLINE for precise timing\n";
    } else {
        db<Component>(INF) << "[Component] [" << _address.to_string() << "] " << _name << " will use SCHED_FIFO (deadline scheduling not available)\n";
    }
    
    _producer_thread_running.store(true, std::memory_order_release);
    int result = pthread_create(&_producer_thread, nullptr, producer_thread_launcher, this);
    if (result != 0) {
        _producer_thread_running.store(false, std::memory_order_release);
        db<Component>(ERR) << "[Component] [" << _address.to_string() << "] " << _name << " failed to create producer thread\n";
        return;
    }
    
    db<Component>(INF) << "[Component] [" << _address.to_string() << "] " << _name << " started producer thread\n";
}

// Stop the producer response thread
void Component::stop_producer_thread() {
    db<Component>(TRC) << "[Component] [" << _address.to_string() << "] " << _name << " stopping producer thread...\n";
    
    if (!_producer_thread_running.load(std::memory_order_acquire)) {
        db<Component>(WRN) << "[Component] [" << _address.to_string() << "] " << _name << " producer thread not running\n";
        return;
    }
    
    // Set the flag to false first to signal the thread to exit
    _producer_thread_running.store(false, std::memory_order_release);
    
    // Only attempt to join if thread handle is valid
    if (_producer_thread != 0) {
        // Set a timeout for joining the thread
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 2; // 2 second timeout
        
        db<Component>(TRC) << "[Component] [" << _address.to_string() << "] " << _name << " waiting for producer thread to join...\n";
        
        // Try to join the thread with timeout
        int join_result = pthread_timedjoin_np(_producer_thread, nullptr, &timeout);
        
        if (join_result == 0) {
            db<Component>(INF) << "[Component] [" << _address.to_string() << "] " << _name << " producer thread joined successfully\n";
        } else if (join_result == ETIMEDOUT) {
            db<Component>(WRN) << "[Component] [" << _address.to_string() << "] " << _name << " producer thread join timed out after 2s\n";
            // Thread might be blocked in a system call - try sending a signal
            pthread_kill(_producer_thread, SIGUSR1);
            // Try joining again with a short timeout
            clock_gettime(CLOCK_REALTIME, &timeout);
            timeout.tv_sec += 1; // 1 more second
            if (pthread_timedjoin_np(_producer_thread, nullptr, &timeout) == 0) {
                db<Component>(INF) << "[Component] [" << _address.to_string() << "] " << _name << " producer thread joined after signal\n";
            } else {
                db<Component>(ERR) << "[Component] [" << _address.to_string() << "] " << _name << " producer thread could not be joined, potential resource leak\n";
                // In a production system we might need a more robust solution, but for now we'll continue
            }
        } else {
            db<Component>(ERR) << "[Component] [" << _address.to_string() << "] " << _name << " pthread_timedjoin_np error: " << strerror(join_result) << "\n";
        }
        
        // Clear the thread ID regardless of join result
        _producer_thread = 0;
    }
    
    db<Component>(INF) << "[Component] [" << _address.to_string() << "] " << _name << " producer thread stop completed\n";
}

// Producer thread entry point
void* Component::producer_thread_launcher(void* context) {
    Component* component = static_cast<Component*>(context);
    if (component) {
        try {
            db<Component>(TRC) << "[Component] [" << component->_address.to_string() << "] Producer thread starting for " 
                             << component->getName() << "\n";
            component->producer_routine();
        } catch (const std::exception& e) {
            db<Component>(ERR) << "[Component] [" << component->_address.to_string() << "] " << component->getName() 
                             << " producer thread exception: " << e.what() << "\n";
        }
    }
    return nullptr;
}

// Producer routine - generates periodic responses based on interests
void Component::producer_routine() {
    db<Component>(TRC) << "[Component] [" << _address.to_string() << "] " << _name << " producer routine started\n";
    
    // Use deadline scheduling if available
    if (_has_dl_capability.load(std::memory_order_relaxed)) {
        // SCHED_DEADLINE setup
        struct sched_attr attr_dl;
        memset(&attr_dl, 0, sizeof(attr_dl));
        attr_dl.size = sizeof(attr_dl);
        attr_dl.sched_policy = SCHED_DEADLINE;
        attr_dl.sched_flags = 0;
        
        struct timespec next_period;
        clock_gettime(CLOCK_MONOTONIC, &next_period);
        
        while (_producer_thread_running.load(std::memory_order_acquire)) {
            // Get the current GCD period
            std::uint32_t current_period = _current_gcd_period_us.load(std::memory_order_acquire);
            
            // If we have no interests yet, sleep and wait
            if (current_period == 0 || _interest_periods.empty()) {
                db<Component>(TRC) << "[Component] [" << _address.to_string() << "] " << _name 
                                 << " no interests yet, sleeping (current period: " << current_period
                                 << ", interest periods count: " << _interest_periods.size() << ")\n";
                // Use shorter sleep interval to check running state more frequently
                for (int i = 0; i < 5 && _producer_thread_running.load(std::memory_order_acquire); i++) {
                    usleep(20000); // 20ms * 5 = 100ms total, but more responsive to shutdown
                }
                continue;
            }
            
            db<Component>(TRC) << "[Component] [" << _address.to_string() << "] " << _name 
                           << " preparing to send response with period " << current_period << "us\n";
            
            // Generate the response data only if we're still running
            if (_producer_thread_running.load(std::memory_order_acquire)) {
                // Update SCHED_DEADLINE parameters based on current period
                attr_dl.sched_runtime = current_period * 500; // 50% of period in ns
                attr_dl.sched_deadline = current_period * 1000; // Period in ns
                attr_dl.sched_period = current_period * 1000; // Period in ns
                
                // Set scheduling parameters - may fail if not root
                int result = sched_setattr(0, &attr_dl, 0);
                if (result < 0) {
                    db<Component>(WRN) << "[Component] [" << _address.to_string() << "] " << _name 
                                    << " failed to set SCHED_DEADLINE, falling back to SCHED_FIFO\n";
                    _has_dl_capability.store(false, std::memory_order_relaxed);
                    
                    // Fall back to SCHED_FIFO
                    struct sched_param fifo_param;
                    fifo_param.sched_priority = 99; // Max RT priority
                    pthread_setschedparam(pthread_self(), SCHED_FIFO, &fifo_param);
                    // Continue with the next loop iteration which will use the FIFO path
                    continue;
                }
                
                // Generate response based on the current period
                std::vector<std::uint8_t> response_data;
                if (produce_data_for_response(_produced_data_type, response_data) && 
                    _producer_thread_running.load(std::memory_order_acquire)) {
                    // Create response message with the data
                    Message response = _communicator->new_message(
                        Message::Type::RESPONSE, 
                        _produced_data_type,
                        0,  // period is 0 for responses
                        response_data.data(),
                        response_data.size()
                    );
                    
                    // Send to Gateway for broadcast distribution
                    // Only if we're still running (last check before sending)
                    if (_producer_thread_running.load(std::memory_order_acquire)) {
                        Address gateway_addr(Ethernet::BROADCAST, GATEWAY_PORT);
                        _communicator->send(response, gateway_addr);
                        
                        db<Component>(INF) << "[Component] [" << _address.to_string() << "] " << _name << " sent RESPONSE for data type " 
                                        << static_cast<int>(_produced_data_type) << " with " 
                                        << response_data.size() << " bytes\n";
                        
                        // Log RESPONSE_SENT event to CSV
                        if (_log_file.is_open()) {
                            auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                            _log_file << now_us << ","                                 // timestamp_us
                                     << "PRODUCER" << ","                             // event_category
                                     << "RESPONSE_SENT" << ","                   // event_type
                                     << response.timestamp() << ","                // message_id
                                     << static_cast<int>(Message::Type::RESPONSE) << ","// message_type
                                     << static_cast<int>(response.unit_type()) << "," // data_type_id
                                     << address().to_string() << ","                // origin_address (Component's own)
                                     << gateway_addr.to_string() << ","           // destination_address (Gateway)
                                     << "0" << ","                                // period_us
                                     << response.value_size() << ","               // value_size
                                     << "-"                                      // notes
                                     << "\n";
                            _log_file.flush();
                        }
                    }
                } else {
                    db<Component>(ERR) << "[Component] [" << _address.to_string() << "] " << _name 
                                    << " failed to produce data for response type " 
                                    << static_cast<int>(_produced_data_type) << "\n";
                }
            }
            
            // Instead of sleeping for a full period at once, split into smaller chunks
            // to remain responsive to shutdown requests
            const unsigned MAX_SLEEP_US = 50000; // 50ms chunks maximum
            unsigned remaining_us = current_period;
            
            while (remaining_us > 0 && _producer_thread_running.load(std::memory_order_acquire)) {
                unsigned sleep_chunk = std::min(remaining_us, MAX_SLEEP_US);
                usleep(sleep_chunk);
                remaining_us -= sleep_chunk;
            }
            
            // Calculate next period time
            next_period.tv_nsec += current_period * 1000;
            if (next_period.tv_nsec >= 1000000000) {
                next_period.tv_sec += next_period.tv_nsec / 1000000000;
                next_period.tv_nsec %= 1000000000;
            }
        }
    } else {
        // SCHED_FIFO fallback
        struct sched_param fifo_param;
        fifo_param.sched_priority = 99; // Max RT priority
        pthread_setschedparam(pthread_self(), SCHED_FIFO, &fifo_param);
        
        // Response generation loop
        while (_producer_thread_running.load(std::memory_order_acquire)) {
            // Get the current GCD period
            std::uint32_t current_period = _current_gcd_period_us.load(std::memory_order_acquire);
            
            // If we have no interests yet, sleep and wait
            if (current_period == 0 || _interest_periods.empty()) {
                db<Component>(TRC) << "[Component] [" << _address.to_string() << "] " << _name 
                                 << " no interests yet, sleeping (current period: " << current_period
                                 << ", interest periods count: " << _interest_periods.size() << ")\n";
                // Use shorter sleep interval to check running state more frequently
                for (int i = 0; i < 5 && _producer_thread_running.load(std::memory_order_acquire); i++) {
                    usleep(20000); // 20ms * 5 = 100ms total, but more responsive to shutdown
                }
                continue;
            }
            
            // Generate response only if we're still running
            if (_producer_thread_running.load(std::memory_order_acquire)) {
                db<Component>(TRC) << "[Component] [" << _address.to_string() << "] " << _name 
                                << " preparing to send response with period " << current_period << "us\n";
                
                // Generate response based on the current period
                std::vector<std::uint8_t> response_data;
                if (produce_data_for_response(_produced_data_type, response_data) && 
                    _producer_thread_running.load(std::memory_order_acquire)) {
                    // Create response message with the data
                    Message response = _communicator->new_message(
                        Message::Type::RESPONSE, 
                        _produced_data_type,
                        0,  // period is 0 for responses
                        response_data.data(),
                        response_data.size()
                    );
                    
                    // Send to Gateway for broadcast distribution
                    // Only if we're still running (last check before sending)
                    if (_producer_thread_running.load(std::memory_order_acquire)) {
                        Address gateway_addr(Ethernet::BROADCAST, GATEWAY_PORT);
                        _communicator->send(response, gateway_addr);
                        
                        db<Component>(INF) << "[Component] [" << _address.to_string() << "] " << _name << " sent RESPONSE for data type " 
                                        << static_cast<int>(_produced_data_type) << " with " 
                                        << response_data.size() << " bytes\n";
                        
                        // Log RESPONSE_SENT event to CSV (SCHED_FIFO path)
                        if (_log_file.is_open()) {
                            auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                             _log_file << now_us << ","                                // timestamp_us
                                     << "PRODUCER" << ","                            // event_category
                                     << "RESPONSE_SENT" << ","                  // event_type
                                     << response.timestamp() << ","               // message_id
                                     << static_cast<int>(Message::Type::RESPONSE) << ","// message_type
                                     << static_cast<int>(response.unit_type()) << ","// data_type_id
                                     << address().to_string() << ","               // origin_address (Component's own)
                                     << gateway_addr.to_string() << ","          // destination_address (Gateway)
                                     << "0" << ","                               // period_us
                                     << response.value_size() << ","              // value_size
                                     << "-"                                     // notes
                                     << "\n";
                            _log_file.flush();
                        }
                    }
                } else if (_producer_thread_running.load(std::memory_order_acquire)) {
                    db<Component>(ERR) << "[Component] [" << _address.to_string() << "] " << _name 
                                    << " failed to produce data for response type " 
                                    << static_cast<int>(_produced_data_type) << "\n";
                }
            }
            
            // Split sleep into smaller chunks to be more responsive to shutdown
            const unsigned MAX_SLEEP_US = 50000; // 50ms chunks maximum
            unsigned remaining_us = current_period;
            
            while (remaining_us > 0 && _producer_thread_running.load(std::memory_order_acquire)) {
                unsigned sleep_chunk = std::min(remaining_us, MAX_SLEEP_US);
                usleep(sleep_chunk);
                remaining_us -= sleep_chunk;
            }
        }
    }
    
    db<Component>(TRC) << "[Component] [" << _address.to_string() << "] " << _name << " producer routine exiting\n";
}

// Calculate the greatest common divisor (GCD) of all interest periods
std::uint32_t Component::update_gcd_period() {
    if (_interest_periods.empty()) {
        return 0;
    }
    
    if (_interest_periods.size() == 1) {
        return _interest_periods[0];
    }
    
    // Calculate GCD of all periods
    std::uint32_t result = _interest_periods[0];
    for (size_t i = 1; i < _interest_periods.size(); i++) {
        result = calculate_gcd(result, _interest_periods[i]);
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

#endif // COMPONENT_H 