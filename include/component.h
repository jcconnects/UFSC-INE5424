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

// Define syscall functions for sched_deadline
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

        // Common members
        const Vehicle* _vehicle;
        std::string _name;
        std::atomic<bool> _running; // Overall component operational status
        pthread_t _thread; // Original main thread for run()

        // Type-safe communicator storage
        Comms* _communicator;
        Address _gateway_address; // Address of the gateway component

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
        
        // Virtual method for generating response data - Producers will override
        virtual bool produce_data_for_response(DataTypeId type, std::vector<std::uint8_t>& out_value) { return false; }

        // Virtual method that can be overridden by producer components to handle registration acknowledgments
        virtual void on_producer_registration_confirmed() {
            // Base implementation does nothing
        }

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
    db<Component>(TRC) << "[Component] start() called for component " << getName() << ".\n";

    if (running()) {
        db<Component>(WRN) << "[Component] start() called when component" << getName() << " is already running.\n";
        return;
    }


    _gateway_address = Address(_communicator->address().paddr(), 0);
    _running.store(true, std::memory_order_release);
    
    // Start dispatcher thread
    _dispatcher_running.store(true);
    int disp_rc = pthread_create(&_component_dispatcher_thread_id, nullptr, 
                                Component::component_dispatcher_launcher, this);
    if (disp_rc) {
        db<Component>(ERR) << "[Component] Failed to create dispatcher thread for " << getName() 
                          << ", error code: " << disp_rc << "\n";
        _dispatcher_running.store(false);
        _running.store(false);
        throw std::runtime_error("Failed to create dispatcher thread for " + getName());
    }
    db<Component>(INF) << "[Component] " << getName() << " dispatcher thread created successfully.\n";
    
    // Create main thread for run() method
    int rc = pthread_create(&_thread, nullptr, Component::thread_entry_point, this);
    if (rc) {
        db<Component>(ERR) << "[Component] return code from pthread_create() is " << rc << " for "<< getName() << "\n";
        
        // Clean up dispatcher thread if main thread creation fails
        _dispatcher_running.store(false);
        pthread_join(_component_dispatcher_thread_id, nullptr);
        _component_dispatcher_thread_id = 0;
        
        _running.store(false);
        throw std::runtime_error("Failed to create component thread for " + getName());
    }
    db<Component>(INF) << "[Component] " << getName() << " thread created successfully.\n";
}

void Component::stop() {
    db<Component>(TRC) << "[Component] stop() called for component " << getName() << "\n";

    if (!running()) {
        db<Component>(WRN) << "[Component] stop() called for component " << getName() << " when it is already stopped.\n";
        return;
    }

    // Set running state to false
    _running.store(false, std::memory_order_release);
    
    // Stop dispatcher thread
    _dispatcher_running.store(false);
    
    // Stop producer thread if running
    if (_producer_thread_running.load()) {
        stop_producer_response_thread();
    }
    
    // Then stops communicator, releasing any blocked threads
    _communicator->close();
    db<Component>(INF) << "[Component] Communicator closed for component" << getName() << ".\n";
    
    // Join dispatcher thread
    if (_component_dispatcher_thread_id != 0) {
        int join_rc = pthread_join(_component_dispatcher_thread_id, nullptr);
        if (join_rc == 0) {
            db<Component>(INF) << "[Component] Dispatcher thread joined for component " << getName() << ".\n";
        } else {
            db<Component>(ERR) << "[Component] Failed to join dispatcher thread for component " 
                              << getName() << "! Error code: " << join_rc << "\n";
        }
        _component_dispatcher_thread_id = 0;
    }
    
    // Stop and join all handler threads
    for (auto& handler : _typed_data_handlers) {
        handler->stop_processing_thread();
    }
    
    for (pthread_t thread_id : _handler_threads_ids) {
        if (thread_id != 0) {
            pthread_join(thread_id, nullptr);
        }
    }
    
    // Clear handlers and thread IDs
    _typed_data_handlers.clear();
    _handler_threads_ids.clear();
    
    // Then join main thread
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

ComponentType Component::determine_component_type() const {
    // Port 0 is always a Gateway
    if (_communicator && _communicator->address().port() == 0) {
        return ComponentType::GATEWAY;
    }
    
    // Check if component is a producer
    bool is_producer = (_produced_data_type != DataTypeId::UNKNOWN);
    
    // Check if component is a consumer
    bool is_consumer = !_active_interests.empty();
    
    if (is_producer && is_consumer) {
        return ComponentType::PRODUCER_CONSUMER;
    } else if (is_producer) {
        return ComponentType::PRODUCER;
    } else if (is_consumer) {
        return ComponentType::CONSUMER;
    }
                
    return ComponentType::UNKNOWN;
}

int Component::send(const void* data, unsigned int size, Address destination) {
    
    // Create response message with the data
    Message msg = _communicator->new_message(
        Message::Type::RESPONSE,  // Using RESPONSE type for raw data
        DataTypeId::UNKNOWN,      // Changed from 0 to DataTypeId::UNKNOWN
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
int Component::receive(Message* msg) {
    db<Component>(TRC) << "[Component] " << getName() << " receive called!\n";

    if (!_communicator->receive(msg)) {
        // Check if we stopped while waiting (only for log purposes)
        if (!running()) {
            db<Component>(INF) << "[Component] " << getName() << " receive interrupted by stop().\n";
        }

        db<Component>(WRN) << "[Component] " << getName() << " receive failed.\n";
        return 0; // Indicate error
    }

    db<Component>(TRC) << "[Component] " << getName() << " received message of size " << msg->size() << ".\n";

    // For RESPONSE messages, check the value
    if (msg->message_type() == Message::Type::RESPONSE) {
        db<Component>(TRC) << "[Component] " << getName() << " received RESPONSE message.\n";
        return msg->size(); // Indicate errora
    }
    
    // For any other message type or if no value data
    unsigned int msg_size = msg->size();
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
    struct stat info;
    if (stat("/app/logs", &info) == 0 && (info.st_mode & S_IFDIR)) {
        log_dir = "/app/logs/vehicle_" + std::to_string(vehicle_id) + "/";
    } else {
        // Try to use tests/logs directory instead of current directory
        if (stat("tests/logs", &info) != 0 || !(info.st_mode & S_IFDIR)) {
            try {
                // Create directory with permissions 0755 (rwxr-xr-x)
                mkdir("tests/logs", 0755);
            } catch (...) {
                // Ignore errors, will fall back if needed
            }
        }
        
        if (stat("tests/logs", &info) == 0 && (info.st_mode & S_IFDIR)) {
            log_dir = "tests/logs/vehicle_" + std::to_string(vehicle_id) + "/";
        } else {
            // Last resort fallback to current directory
            log_dir = "./";
        }
    }
    
    // Create the directory if it doesn't exist
    if (log_dir != "./") {
        try {
            // Create vehicle-specific directory with permissions 0755 (rwxr-xr-x)
            mkdir(log_dir.c_str(), 0755);
        } catch (...) {
            // If we can't create the directory, fall back to current directory
            log_dir = "./";
        }
    }
    
    return log_dir;
}

// Implement register_interest_handler
void Component::register_interest_handler(DataTypeId type, std::uint32_t period_us, std::function<void(const Message&)> callback) {
    db<Component>(TRC) << "[Component] register_interest_handler(" << static_cast<int>(type) << ", " << period_us << ")\n";
    
    // Create a new interest request
    InterestRequest request;
    request.type = type;
    request.period_us = period_us;
    request.last_accepted_response_time_us = 0;
    request.interest_sent = false;
    request.callback = callback;
    
    // Add to active interests
    _active_interests.push_back(request);
    
    // Create a TypedDataHandler for this interest
    auto handler = std::make_unique<TypedDataHandler>(
        type, 
        callback, 
        this, 
        &_internal_typed_observed
    );
    
    // Start the handler's processing thread
    handler->start_processing_thread();
    
    // Store the thread ID for later joining
    _handler_threads_ids.push_back(handler->get_thread_id());
    
    // Store the handler
    _typed_data_handlers.push_back(std::move(handler));
    
    // Send initial interest message
    Message interest_msg = _communicator->new_message(
        Message::Type::INTEREST,
        type,
        period_us
    );
    
    // Send to broadcast address
    _communicator->add_interest(type, period_us);
    _communicator->send(interest_msg, Address::BROADCAST);
    _communicator->send(interest_msg, _gateway_address);
    
    // Mark interest as sent
    _active_interests.back().interest_sent = true;
    
    db<Component>(INF) << "[Component] " << getName() << " registered interest in data type " 
                      << static_cast<int>(type) << " with period " << period_us << " microseconds\n";
}

// Implement component_dispatcher_launcher
void* Component::component_dispatcher_launcher(void* context) {
    Component* self = static_cast<Component*>(context);
    if (self) {
        db<Component>(INF) << "[Component] " << self->getName() << " dispatcher thread starting.\n";
        self->component_dispatcher_routine();
        db<Component>(INF) << "[Component] " << self->getName() << " dispatcher thread exiting.\n";
    }
    return nullptr;
}

// Implement component_dispatcher_routine
void Component::component_dispatcher_routine() {
    db<Component>(TRC) << "[Component] " << getName() << " dispatcher routine started.\n";
    
    // Buffer for raw messages
    std::uint8_t raw_buffer[1024]; // Adjust size as needed based on your MTU
    
    while (_dispatcher_running.load()) {
        // Receive raw message
        Message message = _communicator->new_message(Message::Type::RESPONSE, DataTypeId::UNKNOWN); // Changed from 0 to DataTypeId::UNKNOWN
        int recv_size = receive(&message);
        
        if (recv_size <= 0) {
            // Check if we should exit
            if (!_dispatcher_running.load()) {
                break;
            }
            
            // Handle error or timeout
            if (recv_size < 0) {
                db<Component>(ERR) << "[Component] " << getName() << " dispatcher receive error: " << recv_size << "\n";
            }
            
            // Continue to next iteration
            continue;
        }
        
        try {
            
            // Handle REG_PRODUCER_ACK messages for producers
            if (message.message_type() == Message::Type::REG_PRODUCER_ACK) {
                // Check if this component is a producer and verify the data type
                if ((determine_component_type() == ComponentType::PRODUCER || 
                     determine_component_type() == ComponentType::PRODUCER_CONSUMER) &&
                    message.unit_type() == _produced_data_type) {
                    
                    db<Component>(INF) << "[Component] " << getName() << " received REG_PRODUCER_ACK for type " 
                                      << static_cast<int>(_produced_data_type) << "\n";
                    // Call the virtual method instead of dynamic_cast
                    on_producer_registration_confirmed();
                }
            }
            
            // Producer-specific INTEREST handling logic for GCD updates
            // Check if this is a producer component and if the message is an INTEREST for our data type
            if (_produced_data_type != DataTypeId::UNKNOWN && 
                message.message_type() == Message::Type::INTEREST &&
                message.unit_type() == _produced_data_type) {
                
                db<Component>(INF) << "[Component] " << getName() << " component_dispatcher_routine received INTEREST for its produced type " << static_cast<int>(_produced_data_type) << " from origin " << message.origin().to_string() << " with period " << message.period() << "us\n";

                // Add this period to our received_interest_periods if not already present
                std::uint32_t requested_period = message.period();
                bool period_exists = false;
                
                for (std::uint32_t existing_period : _received_interest_periods) {
                    if (existing_period == requested_period) {
                        period_exists = true;
                        break;
                    }
                }
                
                if (!period_exists) {
                    _received_interest_periods.push_back(requested_period);
                    std::uint32_t old_gcd_period = update_gcd_period();
                    std::uint32_t new_gcd_period = _current_gcd_period_us.load();

                    if (new_gcd_period != old_gcd_period) {
                        db<Component>(INF) << "[Component] " << getName() << " dispatcher: GCD changed. Old: " << old_gcd_period 
                                          << "us, New: " << new_gcd_period << "us. Managing response thread.\n";
                        if (_producer_thread_running.load()) {
                            db<Component>(INF) << "[Component] " << getName() << " dispatcher: Stopping existing response thread due to GCD change.\n";
                            stop_producer_response_thread(); // Signals and joins
                        }
                        // If new GCD is valid (>0), a new thread will be started (or restarted)
                        if (new_gcd_period > 0) {
                            db<Component>(INF) << "[Component] " << getName() << " dispatcher: Starting/restarting response thread with new GCD " << new_gcd_period << "us.\n";
                            start_producer_response_thread();
                        } else {
                            db<Component>(INF) << "[Component] " << getName() << " dispatcher: New GCD is 0, response thread remains stopped.\n";
                        }
                    } else if (new_gcd_period > 0 && !_producer_thread_running.load()) {
                        // GCD didn't change from a non-zero value, but thread wasn't running (e.g. first interest, or interests were cleared and now one is added back)
                        db<Component>(INF) << "[Component] " << getName() << " dispatcher: GCD is " << new_gcd_period 
                                          << "us and thread not running. Starting response thread.\n";
                        start_producer_response_thread();
                    } else if (new_gcd_period == 0 && _producer_thread_running.load()) {
                        // This case should ideally be handled if all interests are removed, leading to GCD=0
                        db<Component>(INF) << "[Component] " << getName() << " dispatcher: GCD became 0 and thread is running. Stopping response thread.\n";
                        stop_producer_response_thread();
                    }
                }
            }
            
            // Create heap-allocated message for observers
            Message* heap_msg = new Message(message);
            
            // Get the message type for routing
            DataTypeId msg_type = heap_msg->unit_type();
            
            // Notify appropriate typed observers
            bool delivered = _internal_typed_observed.notify(msg_type, heap_msg);
            
            if (!delivered) {
                // No observer for this type, clean up the heap message
                delete heap_msg;
                db<Component>(INF) << "[Component] " << getName() << " received message of type " 
                                  << static_cast<int>(msg_type) << " but no handler is registered.\n";
            }
        } catch (const std::exception& e) {
            db<Component>(ERR) << "[Component] " << getName() << " dispatcher exception: " << e.what() << "\n";
        }
    }
    
    db<Component>(TRC) << "[Component] " << getName() << " dispatcher routine exiting.\n";
}

// Implement producer_response_launcher
void* Component::producer_response_launcher(void* context) {
    Component* self = static_cast<Component*>(context);
    if (self) {
        db<Component>(INF) << "[Component] " << self->getName() << " producer response thread starting.\n";
        self->producer_response_routine();
        db<Component>(INF) << "[Component] " << self->getName() << " producer response thread exiting.\n";
    }
    return nullptr;
}

// Implement producer_response_routine
void Component::producer_response_routine() {
    db<Component>(TRC) << "[Component] " << getName() << " producer response routine started.\n";
    
    // Set up clock for timing
    struct timespec next_period;
    clock_gettime(CLOCK_MONOTONIC, &next_period);
    
    while (_producer_thread_running.load()) {
        // Check if we have a valid period to use
        std::uint32_t current_period = _current_gcd_period_us.load();
        
        if (current_period > 0) {
            // Check if we have SCHED_DEADLINE capability
            if (_has_dl_capability.load()) {
                // Set up SCHED_DEADLINE parameters based on our GCD period
                struct sched_attr attr_dl;
                memset(&attr_dl, 0, sizeof(attr_dl));
                attr_dl.size = sizeof(attr_dl);
                attr_dl.sched_policy = SCHED_DEADLINE;
                
                // Runtime is the execution time allocated for this task (in ns)
                // We'll allocate 80% of the period for runtime to be safe
                uint64_t runtime_ns = (current_period * 1000) * 0.8;
                
                // Period and deadline are set to the GCD period (in ns)
                uint64_t period_ns = current_period * 1000;
                
                attr_dl.sched_runtime = runtime_ns;
                attr_dl.sched_period = period_ns;
                attr_dl.sched_deadline = period_ns; // deadline = period for our use case
                
                db<Component>(TRC) << "[Component] " << getName() << " attempting to set SCHED_DEADLINE with P=" << period_ns << ", D=" << attr_dl.sched_deadline << ", R=" << attr_dl.sched_runtime << " ns\n";
                // Apply SCHED_DEADLINE to current thread
                int ret = sched_setattr(0, &attr_dl, 0);
                if (ret < 0) {
                    db<Component>(WRN) << "[Component] " << getName() 
                                     << " failed to set SCHED_DEADLINE (errno " << errno 
                                     << "), falling back to usleep-based timing for period " << current_period << " us.\n";
                    _has_dl_capability.store(false); // Update capability state
                } else {
                    db<Component>(INF) << "[Component] " << getName() << " SCHED_DEADLINE set successfully for period " << current_period << " us.\n";
                    // Using SCHED_DEADLINE for timing - the kernel will handle the scheduling
                    
                    while (_producer_thread_running.load() && _current_gcd_period_us.load() == current_period) {
                        // Generate response data
                        std::vector<std::uint8_t> response_data;
                        
                        // Call the virtual method that derived classes will override
                        if (produce_data_for_response(_produced_data_type, response_data)) {
                            // Create a RESPONSE message
                            Message response_msg = _communicator->new_message(
                                Message::Type::RESPONSE,
                                _produced_data_type,
                                0, // No period for responses
                                response_data.data(),
                                response_data.size()
                            );
                            
                            db<Component>(INF) << "[Component] " << getName() << " sending RESPONSE (via SCHED_DEADLINE path) for type " << static_cast<int>(_produced_data_type) << " with " << response_data.size() << " bytes. Current GCD: " << current_period << " us.\n";
                            // Send to broadcast address
                            _communicator->send(response_msg, Address::BROADCAST);
                            _communicator->send(response_msg, _gateway_address);
                            
                            // db<Component>(INF) << "Component " << getName() << " sent RESPONSE for data type " 
                            //                   << static_cast<int>(_produced_data_type) << " with " 
                            //                   << response_data.size() << " bytes.\n";
                        } else {
                            db<Component>(WRN) << "Producer " << getName() << " failed to produce data (SCHED_DEADLINE path) for type " 
                                              << static_cast<int>(_produced_data_type) << ".\n";
                        }
                        
                        // Let the SCHED_DEADLINE scheduler handle the timing
                        sched_yield();
                    }
                    
                    // Reset to normal scheduling when we exit the inner loop
                    struct sched_param param;
                    param.sched_priority = 0;
                    pthread_setschedparam(pthread_self(), SCHED_OTHER, &param);
                    continue; // Skip the fallback code below
                }
            }
            
            // Fallback to usleep-based timing
            // Generate response data
            std::vector<std::uint8_t> response_data;
            
            // Call the virtual method that derived classes will override
            if (produce_data_for_response(_produced_data_type, response_data)) {
                // Create a RESPONSE message
                Message response_msg = _communicator->new_message(
                    Message::Type::RESPONSE,
                    _produced_data_type,
                    0, // No period for responses
                    response_data.data(),
                    response_data.size()
                );
                
                db<Component>(INF) << "[Component] " << getName() << " sending RESPONSE (via usleep path) for type " << static_cast<int>(_produced_data_type) << " with " << response_data.size() << " bytes. Current GCD: " << current_period << " us.\n";
                // Send to broadcast address
                _communicator->send(response_msg, Address::BROADCAST);
                _communicator->send(response_msg, _gateway_address);
                
                // db<Component>(INF) << "Component " << getName() << " sent RESPONSE for data type " 
                //                   << static_cast<int>(_produced_data_type) << " with " 
                //                   << response_data.size() << " bytes.\n";
            } else {
                db<Component>(WRN) << "[Component] " << getName() << " failed to produce data (usleep path) for type " 
                                  << static_cast<int>(_produced_data_type) << ".\n";
            }
            
            // Sleep for the current GCD period using usleep (fallback)
            usleep(current_period);
        } else {
            // No valid period, sleep a bit and check again
            usleep(100000); // 100ms
        }
    }
    
    db<Component>(TRC) << "[Component] " << getName() << " producer response routine exiting.\n";
}

// Implement start_producer_response_thread
void Component::start_producer_response_thread() {
    db<Component>(TRC) << "[Component] " << getName() << " start_producer_response_thread() called.\n";
    
    // Only start if not already running
    if (!_producer_thread_running.load() && _produced_data_type != DataTypeId::UNKNOWN) {
        _producer_thread_running.store(true);
        
        // Check for SCHED_DEADLINE capability
        _has_dl_capability.store(has_deadline_scheduling_capability());
        if (_has_dl_capability.load()) {
            db<Component>(INF) << "Component " << getName() << " has SCHED_DEADLINE capability.\n";
        } else {
            db<Component>(WRN) << "Component " << getName() << " will fall back to usleep-based timing.\n";
        }
        
        // Create thread attributes
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        
        // Set detach state to joinable (default, but being explicit)
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
        
        // Create the thread with default settings - SCHED_DEADLINE will be set inside the thread
        int rc = pthread_create(&_producer_response_thread_id, &attr, 
                               Component::producer_response_launcher, this);
        
        // Clean up thread attributes
        pthread_attr_destroy(&attr);
        
        if (rc) {
            db<Component>(ERR) << "[Component] Failed to create producer thread for " << getName() 
                              << ", error code: " << rc << "\n";
            _producer_thread_running.store(false);
            _producer_response_thread_id = 0;
        } else {
            db<Component>(INF) << "[Component] " << getName() << " producer thread created successfully.\n";
        }
    } else {
        if (_producer_thread_running.load()) {
            db<Component>(WRN) << "[Component] Producer thread already running for " << getName() << ".\n";
        } else {
            db<Component>(WRN) << "[Component] Not a producer component (no data type set) for " << getName() << ".\n";
        }
    }
}

// Implement stop_producer_response_thread
void Component::stop_producer_response_thread() {
    db<Component>(TRC) << "[Component] " << getName() << " stop_producer_response_thread() called.\n";
    
    if (_producer_thread_running.load()) {
        // Signal the thread to stop
        _producer_thread_running.store(false);
        
        // Join the thread
        if (_producer_response_thread_id != 0) {
            int join_rc = pthread_join(_producer_response_thread_id, nullptr);
            
            if (join_rc == 0) {
                db<Component>(INF) << "[Component] " << getName() << " producer thread joined.\n";
            } else {
                db<Component>(ERR) << "[Component] " << getName() << " failed to join producer thread! Error code: " << join_rc << "\n";
            }
            
            _producer_response_thread_id = 0;
        }
    } else {
        db<Component>(WRN) << "[Component] stop_producer_response_thread() called when producer thread is not running for " 
                         << getName() << ".\n";
    }
}

// Implement update_gcd_period
std::uint32_t Component::update_gcd_period() {
    db<Component>(TRC) << "[Component] " << getName() << " update_gcd_period() called.\n";
    
    std::uint32_t old_gcd_period = _current_gcd_period_us.load(); // Store old GCD
    std::uint32_t new_gcd_period = 0;

    // If no periods, set GCD to 0
    if (_received_interest_periods.empty()) {
        _current_gcd_period_us.store(0);
        db<Component>(INF) << "[Component] " << getName() << " has no active interests. GCD set to 0 us.\n";
        return old_gcd_period; // Return old GCD
    }
    
    // Start with the first period
    new_gcd_period = _received_interest_periods[0];
    
    // Calculate GCD of all periods
    for (size_t i = 1; i < _received_interest_periods.size(); ++i) {
        new_gcd_period = calculate_gcd(new_gcd_period, _received_interest_periods[i]);
    }
    
    // Store the result
    _current_gcd_period_us.store(new_gcd_period);
    
    if (new_gcd_period != old_gcd_period) {
        db<Component>(INF) << "[Component] " << getName() << " updated GCD period from " << old_gcd_period << "us to " 
                          << new_gcd_period << " microseconds.\n";
    } else {
        db<Component>(TRC) << "[Component] " << getName() << " GCD period remains " << new_gcd_period << " microseconds.\n";
    }
    return old_gcd_period; // Return old GCD
}

// Implement calculate_gcd
std::uint32_t Component::calculate_gcd(std::uint32_t a, std::uint32_t b) {
    // Handle edge cases
    if (a == 0) return b;
    if (b == 0) return a;
    
    // Euclidean algorithm
    while (b != 0) {
        std::uint32_t temp = b;
        b = a % b;
        a = temp;
    }
    
    return a;
}

// Add helper method to check for SCHED_DEADLINE capability
bool Component::has_deadline_scheduling_capability() {
    // Try to set SCHED_DEADLINE parameters with minimal values
    struct sched_attr attr_test;
    memset(&attr_test, 0, sizeof(attr_test));
    attr_test.size = sizeof(attr_test);
    attr_test.sched_policy = SCHED_DEADLINE;
    attr_test.sched_runtime = 50000;        // 50 microsecond runtime
    attr_test.sched_period = 100000;        // 100 microsecond period
    attr_test.sched_deadline = 100000;      // 100 microsecond deadline
    
    // Try setting it on current thread temporarily
    int result = sched_setattr(0, &attr_test, 0);
    
    // If it works, reset back to normal
    if (result == 0) {
        struct sched_param param;
        param.sched_priority = 0;
        pthread_setschedparam(pthread_self(), SCHED_OTHER, &param);
        return true;
    }
    
    // If EPERM, we don't have the capability
    if (errno == EPERM) {
        db<Component>(WRN) << "[Component] " << getName() 
                         << " lacks CAP_SYS_NICE capability for SCHED_DEADLINE. "
                         << "Consider using: sudo setcap cap_sys_nice+ep <executable>\n";
    } else {
        db<Component>(WRN) << "[Component] " << getName()
                         << " SCHED_DEADLINE test failed with error: " << errno << "\n";
    }
    
    return false;
}

#endif // COMPONENT_H 