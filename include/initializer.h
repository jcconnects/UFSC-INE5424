#ifndef INITIALIZER_H
#define INITIALIZER_H

#include "protocol.h"
#include "nic.h"
#include "vehicle.h"
#include <iostream>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <stdexcept>

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
    bool isRunning();
    
    // Get process ID
    pid_t getPid();

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

Initializer::Initializer(const VehicleConfig& config) : _config(config), _vehicle_pid(-1), _running(false) {
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

bool Initializer::isRunning() {
    return _running;
}

pid_t Initializer::getPid() {
    return _vehicle_pid;
}

void Initializer::runVehicleProcess()  {
    // Update the log prefix with the actual PID
    _config.log_prefix = "[PID " + std::to_string(getpid()) + "] ";
    
    std::cout << "Vehicle process " << _config.id << " started (PID " << getpid() << ")." << std::endl;
    
    // Setup proper communication stack instead of using the simplified approach
    setupCommunicationStack<SocketEngine>();
}

template <typename Engine>
void Initializer::setupCommunicationStack()  {
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

#endif // INITIALIZER_H
