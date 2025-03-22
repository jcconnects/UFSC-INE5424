// Initializer.cpp
#include "Initializer.h"
#include <iostream>
#include <signal.h>
#include <sys/wait.h>
#include <chrono>
#include <thread>
#include <stdexcept>

// Initializer implementation
Initializer::Initializer(const VehicleConfig& config)
    : _config(config), _vehicle_pid(-1), _running(false) {
    
    std::cout << "Initializer: Creating vehicle " << config.id 
              << " with message periodicity of " 
              << config.period_ms << " ms." << std::endl;
}

Initializer::~Initializer() {
    if (_running) {
        terminateVehicle();
    }
}

pid_t Initializer::startVehicle() {
    pid_t pid = fork();
    
    if (pid < 0) {
        throw std::runtime_error("Failed to fork vehicle process");
    } 
    else if (pid == 0) {
        // Child process
        // Reset signal handlers
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        
        try {
            runVehicleProcess();
        } 
        catch (const std::exception& e) {
            std::cerr << "Exception in vehicle " << _config.id << ": " << e.what() << std::endl;
            exit(EXIT_FAILURE);
        }
        
        exit(EXIT_SUCCESS);
    } 
    else {
        // Parent process
        _vehicle_pid = pid;
        _running = true;
        return pid;
    }
}

void Initializer::runVehicleProcess() {
    // Update the log prefix with the actual PID
    _config.log_prefix = "[PID " + std::to_string(getpid()) + "] ";
    
    std::cout << "Vehicle process " << _config.id << " started (PID " << getpid() << ")." << std::endl;
    
    // Setup proper communication stack instead of using the simplified approach
    setupCommunicationStack<SocketEngine>();
}

template <typename Engine>
void Initializer::setupCommunicationStack() {
    // Create NIC
    auto nic = new NIC<Engine>();
    
    // Create Protocol and attach to NIC
    auto protocol = new Protocol<NIC<Engine>>(nic);
    
    // Create Vehicle with only NIC and Protocol
    Vehicle vehicle(_config, nic, protocol);
    
    // Vehicle will create its own Communicator in its constructor
    
    // Start vehicle communication
    vehicle.communicate();
}

int Initializer::waitForCompletion() {
    if (!_running) {
        return -1;
    }
    
    int status;
    waitpid(_vehicle_pid, &status, 0);
    
    _running = false;
    
    if (WIFEXITED(status)) {
        std::cout << "Vehicle " << _config.id << " (PID " << _vehicle_pid 
                  << ") exited with status " << WEXITSTATUS(status) << std::endl;
        return WEXITSTATUS(status);
    } 
    else if (WIFSIGNALED(status)) {
        std::cout << "Vehicle " << _config.id << " (PID " << _vehicle_pid 
                  << ") terminated by signal " << WTERMSIG(status) << std::endl;
        return -WTERMSIG(status);
    }
    
    return -1;
}

void Initializer::terminateVehicle() {
    if (_running) {
        kill(_vehicle_pid, SIGTERM);
        waitForCompletion();
    }
}

bool Initializer::isRunning() const {
    return _running;
}

pid_t Initializer::getPid() const {
    return _vehicle_pid;
}

// Vehicle implementation
Vehicle::Vehicle(const Config& config)
    : _config(config), _nic(nullptr), _protocol(nullptr), _communicator(nullptr), _is_communicator_set(false) {
    
    log("Vehicle created");
}

template <typename N, typename P>
Vehicle::Vehicle(const Config& config, N* nic, P* protocol)
    : _config(config), _is_communicator_set(false) {
    
    _nic = static_cast<void*>(nic);
    _protocol = static_cast<void*>(protocol);
    _communicator = nullptr;
    
    log("Vehicle created with NIC and Protocol");
    
    // Create the communicator
    createCommunicator(protocol);
}

Vehicle::~Vehicle() {
    // In this simple implementation, we won't try to free memory
    log("Vehicle destroyed");
}

void Vehicle::communicate() {
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
        
        Message msg(msgContent);
        
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

void Vehicle::log(const std::string& message) {
    if (_config.verbose_logging) {
        std::cout << _config.log_prefix << "[Vehicle " << _config.id << "] " 
                  << message << std::endl;
    }
}

void Vehicle::error(const std::string& message) {
    std::cerr << _config.log_prefix << "[Vehicle " << _config.id << "] ERROR: " 
              << message << std::endl;
}

template <typename P>
void Vehicle::createCommunicator(P* protocol) {
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