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
        int receive(void* data, unsigned int max_size, Address* source_address = nullptr); // Optional source addr

        // --- P3 Consumer Method - Public API for registering interest in a DataTypeId ---
        void register_interest_handler(DataTypeId type, std::uint32_t period_us, std::function<void(const Message&)> callback);

        // P3 Producer/Consumer accessors for Communicator filtering
        DataTypeId get_produced_data_type() const { return _produced_data_type; }
        bool has_active_interests() const { return !_active_interests.empty(); }
        
        // Friend declaration for Communicator to access Component's private members for filtering
        template <typename Channel>
        friend class Communicator;

    protected:
        virtual void specific_start();
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

        // --- P3 Dispatcher Methods ---
        static void* component_dispatcher_launcher(void* context);
        void component_dispatcher_routine();

        // --- P3 Producer Methods ---
        static void* producer_response_launcher(void* context);
        void producer_response_routine();
        void start_producer_response_thread();
        void stop_producer_response_thread();
        void update_gcd_period();
        static std::uint32_t calculate_gcd(std::uint32_t a, std::uint32_t b);
        
        // Virtual method for generating response data - Producers will override
        virtual bool produce_data_for_response(DataTypeId type, std::vector<std::uint8_t>& out_value) { return false; }

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
    specific_start();
}

void Component::specific_start() {
    db<Component>(TRC) << "Component::start() called for component " << getName() << ".\n";

    if (running()) {
        db<Component>(WRN) << "[Component] start() called when component" << getName() << " is already running.\n";
        return;
    }

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
    db<Component>(INF) << "Component " << getName() << " dispatcher thread created successfully.\n";
    
    // Check if this is a producer component and start producer thread if needed
    if (_produced_data_type != DataTypeId::UNKNOWN) {
        start_producer_response_thread();
    }

    // Create main thread for run() method
    int rc = pthread_create(&_thread, nullptr, Component::thread_entry_point, this);
    if (rc) {
        db<Component>(ERR) << "[Component] return code from pthread_create() is " << rc << " for "<< getName() << "\n";
        
        // Clean up dispatcher thread if main thread creation fails
        _dispatcher_running.store(false);
        pthread_join(_component_dispatcher_thread_id, nullptr);
        _component_dispatcher_thread_id = 0;
        
        // Stop producer thread if it was started
        if (_producer_thread_running.load()) {
            stop_producer_response_thread();
        }
        
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
int Component::receive(void* data, unsigned int max_size, Address* source_address) {
    db<Component>(TRC) << "[Component] " << getName() << " receive called!\n";

    // Creating a message for receiving
    Message msg = _communicator->new_message(Message::Type::RESPONSE, DataTypeId::UNKNOWN); // Changed from 0 to DataTypeId::UNKNOWN

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
        unsigned int value_size = msg.value_size(); // Changed from msg.size() to msg.value_size()

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
    db<Component>(TRC) << "Component::register_interest_handler(" << static_cast<int>(type) << ", " << period_us << ")\n";
    
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
    _communicator->add_interest(type);
    _communicator->send(interest_msg, Address::BROADCAST);
    
    // Mark interest as sent
    _active_interests.back().interest_sent = true;
    
    db<Component>(INF) << "Component " << getName() << " registered interest in data type " 
                      << static_cast<int>(type) << " with period " << period_us << " microseconds\n";
}

// Implement component_dispatcher_launcher
void* Component::component_dispatcher_launcher(void* context) {
    Component* self = static_cast<Component*>(context);
    if (self) {
        db<Component>(INF) << "Component " << self->getName() << " dispatcher thread starting.\n";
        self->component_dispatcher_routine();
        db<Component>(INF) << "Component " << self->getName() << " dispatcher thread exiting.\n";
    }
    return nullptr;
}

// Implement component_dispatcher_routine
void Component::component_dispatcher_routine() {
    db<Component>(TRC) << "Component " << getName() << " dispatcher routine started.\n";
    
    // Buffer for raw messages
    std::uint8_t raw_buffer[1024]; // Adjust size as needed based on your MTU
    
    while (_dispatcher_running.load()) {
        // Receive raw message
        Address source;
        int recv_size = receive(raw_buffer, sizeof(raw_buffer), &source);
        
        if (recv_size <= 0) {
            // Check if we should exit
            if (!_dispatcher_running.load()) {
                break;
            }
            
            // Handle error or timeout
            if (recv_size < 0) {
                db<Component>(ERR) << "Component " << getName() << " dispatcher receive error: " << recv_size << "\n";
            }
            
            // Continue to next iteration
            continue;
        }
        
        try {
            // Deserialize raw message
            Message message = Message::deserialize(raw_buffer, recv_size);
            
            // Producer-specific INTEREST handling logic for GCD updates
            // Check if this is a producer component and if the message is an INTEREST for our data type
            if (_produced_data_type != DataTypeId::UNKNOWN && 
                message.message_type() == Message::Type::INTEREST &&
                message.unit_type() == _produced_data_type) {
                
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
                    // Update the GCD period
                    update_gcd_period();
                    
                    db<Component>(INF) << "Producer " << getName() << " received INTEREST for type " 
                                      << static_cast<int>(_produced_data_type) << " with period " 
                                      << requested_period << "us. New GCD: " 
                                      << _current_gcd_period_us.load() << "us\n";
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
                db<Component>(INF) << "Component " << getName() << " received message of type " 
                                  << static_cast<int>(msg_type) << " but no handler is registered.\n";
            }
        } catch (const std::exception& e) {
            db<Component>(ERR) << "Component " << getName() << " dispatcher exception: " << e.what() << "\n";
        }
    }
    
    db<Component>(TRC) << "Component " << getName() << " dispatcher routine exiting.\n";
}

// Implement producer_response_launcher
void* Component::producer_response_launcher(void* context) {
    Component* self = static_cast<Component*>(context);
    if (self) {
        db<Component>(INF) << "Component " << self->getName() << " producer response thread starting.\n";
        self->producer_response_routine();
        db<Component>(INF) << "Component " << self->getName() << " producer response thread exiting.\n";
    }
    return nullptr;
}

// Implement producer_response_routine
void Component::producer_response_routine() {
    db<Component>(TRC) << "Component " << getName() << " producer response routine started.\n";
    
    while (_producer_thread_running.load()) {
        // Check if we have a valid period to use
        std::uint32_t current_period = _current_gcd_period_us.load();
        
        if (current_period > 0) {
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
                
                // Send to broadcast address
                _communicator->send(response_msg, Address::BROADCAST);
                
                db<Component>(INF) << "Component " << getName() << " sent RESPONSE for data type " 
                                  << static_cast<int>(_produced_data_type) << " with " 
                                  << response_data.size() << " bytes.\n";
            } else {
                db<Component>(WRN) << "Component " << getName() << " failed to produce data for type " 
                                  << static_cast<int>(_produced_data_type) << ".\n";
            }
            
            // Sleep for the current GCD period
            usleep(current_period);
        } else {
            // No valid period, sleep a bit and check again
            usleep(100000); // 100ms
        }
    }
    
    db<Component>(TRC) << "Component " << getName() << " producer response routine exiting.\n";
}

// Implement start_producer_response_thread
void Component::start_producer_response_thread() {
    db<Component>(TRC) << "Component::start_producer_response_thread() called.\n";
    
    // Only start if not already running
    if (!_producer_thread_running.load() && _produced_data_type != DataTypeId::UNKNOWN) {
        _producer_thread_running.store(true);
        
        int rc = pthread_create(&_producer_response_thread_id, nullptr, 
                               Component::producer_response_launcher, this);
        
        if (rc) {
            db<Component>(ERR) << "[Component] Failed to create producer thread for " << getName() 
                              << ", error code: " << rc << "\n";
            _producer_thread_running.store(false);
            _producer_response_thread_id = 0;
        } else {
            db<Component>(INF) << "Component " << getName() << " producer thread created successfully.\n";
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
    db<Component>(TRC) << "Component::stop_producer_response_thread() called.\n";
    
    if (_producer_thread_running.load()) {
        // Signal the thread to stop
        _producer_thread_running.store(false);
        
        // Join the thread
        if (_producer_response_thread_id != 0) {
            int join_rc = pthread_join(_producer_response_thread_id, nullptr);
            
            if (join_rc == 0) {
                db<Component>(INF) << "[Component] Producer thread joined for component " << getName() << ".\n";
            } else {
                db<Component>(ERR) << "[Component] Failed to join producer thread for component " 
                                  << getName() << "! Error code: " << join_rc << "\n";
            }
            
            _producer_response_thread_id = 0;
        }
    } else {
        db<Component>(WRN) << "[Component] stop_producer_response_thread() called when producer thread is not running for " 
                         << getName() << ".\n";
    }
}

// Implement update_gcd_period
void Component::update_gcd_period() {
    db<Component>(TRC) << "Component::update_gcd_period() called.\n";
    
    // If no periods, set GCD to 0
    if (_received_interest_periods.empty()) {
        _current_gcd_period_us.store(0);
        return;
    }
    
    // Start with the first period
    std::uint32_t result = _received_interest_periods[0];
    
    // Calculate GCD of all periods
    for (size_t i = 1; i < _received_interest_periods.size(); ++i) {
        result = calculate_gcd(result, _received_interest_periods[i]);
    }
    
    // Store the result
    _current_gcd_period_us.store(result);
    
    db<Component>(INF) << "Component " << getName() << " updated GCD period to " 
                      << result << " microseconds.\n";
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

#endif // COMPONENT_H 