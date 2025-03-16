#include <iostream>
#include <cstdlib>
#include <unistd.h>     // for fork(), getpid()
#include <sys/wait.h>   // for wait()
#include <string>
#include <chrono>       // for std::chrono::milliseconds
#include <thread>       // for std::this_thread::sleep_for
#include <vector>
#include <algorithm>
#include <memory>
#include <signal.h>
#include <sstream>

// --------------------------
// Dummy API classes (Black Box)
// --------------------------

// Message represents a simple array of bytes.
class Message {
public:
    Message(const std::string & content) : content(content) {}
    const std::string& data() const { return content; }
    size_t size() const { return content.size(); }
private:
    std::string content;
};

// NIC simulates the network interface card.
class NIC {
public:
    NIC() {
        // Simulate a unique MAC address for each NIC instance
        _mac_address = generateMacAddress();
        std::cout << "[NIC] NIC initialized with MAC " << _mac_address 
                  << " in process " << getpid() << std::endl;
    }
    
    ~NIC() {
        std::cout << "[NIC] NIC " << _mac_address << " destroyed in process " 
                  << getpid() << std::endl;
    }
    
    std::string address() const { 
        return _mac_address; 
    }
    
    // In a real implementation, this would receive frames from the network
    bool receiveFrame(Message& msg) {
        // Simulated implementation
        return true;
    }
    
private:
    std::string generateMacAddress() {
        std::stringstream ss;
        ss << std::hex << getpid();
        std::string pid_hex = ss.str();
        return "00:11:22:33:" + pid_hex.substr(0, 2) + ":" + 
               (pid_hex.length() > 2 ? pid_hex.substr(2, 2) : "00");
    }
    
    std::string _mac_address;
};

// Protocol implements the communication protocol as a singleton.
class Protocol {
public:
    // Address class to simulate Protocol addresses
    class Address {
    public:
        Address() : _physical_addr(""), _port(0) {}
        Address(const std::string& phys_addr, short port) 
            : _physical_addr(phys_addr), _port(port) {}
        
        static const Address BROADCAST;
        
        bool operator==(const Address& other) const {
            return _physical_addr == other._physical_addr && _port == other._port;
        }
        
        friend std::ostream& operator<<(std::ostream& os, const Address& addr) {
            os << addr._physical_addr << ":" << addr._port;
            return os;
        }
        
    private:
        std::string _physical_addr;
        short _port;
    };
    
    static Protocol& getInstance() {
        static Protocol instance;  // One instance per process.
        return instance;
    }
    
    int send(const Address& from, const Address& to, const Message& msg) {
        std::cout << "[Protocol] (PID " << getpid() << ") Sending message from " 
                  << from << " to " << to << ": " << msg.data() << std::endl;
        return 1; // Success simulation.
    }
    
    int receive(Message& msg, Address* from) {
        msg = Message("Dummy received message");
        *from = Address("00:00:00:00:00:00", 0); // Simulate sender address
        return static_cast<int>(msg.size());
    }
    
    // Methods for attaching/detaching observers
    void attach(void* observer, const Address& address) {
        std::cout << "[Protocol] Observer attached for address " << address << std::endl;
        // In real implementation, would store the observer in a list
    }
    
    void detach(void* observer, const Address& address) {
        std::cout << "[Protocol] Observer detached for address " << address << std::endl;
        // In real implementation, would remove the observer from the list
    }
    
private:
    Protocol() {
        std::cout << "[Protocol] Protocol singleton instantiated in process " << getpid() << std::endl;
    }
    Protocol(const Protocol&) = delete;
    Protocol& operator=(const Protocol&) = delete;
};

// Initialize static member
const Protocol::Address Protocol::Address::BROADCAST("FF:FF:FF:FF:FF:FF", 0);

// Communicator uses Protocol to send and receive messages.
class Communicator {
public:
    Communicator(Protocol* protocol, const Protocol::Address& address) 
        : _protocol(protocol), _address(address) 
    {
        std::cout << "[Communicator] Communicator created with address " << _address 
                  << " in process " << getpid() << std::endl;
        // Attach as an observer to Protocol
        _protocol->attach(this, _address);
    }
    
    ~Communicator() {
        // Detach from Protocol
        _protocol->detach(this, _address);
        std::cout << "[Communicator] Communicator destroyed in process " << getpid() << std::endl;
    }
    
    bool send(const Message& msg) {
        return _protocol->send(_address, Protocol::Address::BROADCAST, msg) > 0;
    }
    
    bool receive(Message& msg) {
        Protocol::Address from;
        return _protocol->receive(msg, &from) > 0;
    }
    
private:
    Protocol* _protocol;
    Protocol::Address _address;
};

// --------------------------
// Vehicle Config Structure
// --------------------------
struct VehicleConfig {
    int id;
    int period_ms;
    bool verbose_logging;
    std::string log_prefix;
};

// --------------------------
// Vehicle class definition
// --------------------------
class Vehicle {
public:
    Vehicle(const VehicleConfig& config) : config(config) {
        log("Vehicle created");
        setupCommunicationPipeline();
    }
    
    ~Vehicle() {
        teardownCommunicationPipeline();
        log("Vehicle destroyed");
    }
    
    void log(const std::string& message) {
        if (config.verbose_logging) {
            std::cout << config.log_prefix << "[Vehicle " << config.id << "] " 
                      << message << std::endl;
        }
    }
    
    void error(const std::string& message) {
        std::cerr << config.log_prefix << "[Vehicle " << config.id << "] ERROR: " 
                  << message << std::endl;
    }
    
    // Sets up the communication pipeline in a black box fashion.
    void setupCommunicationPipeline() {
        try {
            // Create NIC instance first
            nic = new NIC();
            log("NIC initialized with MAC address: " + nic->address());
            
            // Get Protocol singleton (which attaches itself to NIC as an observer in real impl)
            protocol = &Protocol::getInstance();
            log("Protocol accessed");
            
            // Create a unique address for this vehicle's communicator
            communicator_address = Protocol::Address(nic->address(), static_cast<short>(config.id));
            
            // Create communicator and attach it to protocol
            communicator = new Communicator(protocol, communicator_address);
            log("Communicator created with address: " + std::to_string(config.id));
            
            log("Communication pipeline setup complete.");
        }
        catch (const std::exception& e) {
            error("Exception during setup: " + std::string(e.what()));
            teardownCommunicationPipeline();
            throw;
        }
    }
    
    // Tears down the communication pipeline.
    void teardownCommunicationPipeline() {
        // Clean up in reverse order of creation
        if (communicator) {
            delete communicator;
            communicator = nullptr;
            log("Communicator destroyed");
        }
        
        // protocol is a singleton, don't delete
        
        if (nic) {
            delete nic;
            nic = nullptr;
            log("NIC destroyed");
        }
        
        log("Communication pipeline torn down.");
    }
    
    // Simulates periodic communication.
    void communicate() {
        int counter = 0;
        int max_attempts = 3;
        int success_count = 0;
        int failure_count = 0;
        
        log("Beginning communication cycle");
        
        while (counter++ < 10) {
            // Create a message with vehicle ID and timestamp
            auto now = std::chrono::system_clock::now();
            auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count();
            
            std::string msgContent = "Vehicle " + std::to_string(config.id) + 
                                     " message " + std::to_string(counter) + 
                                     " at " + std::to_string(time_ms);
            Message msg(msgContent);
            
            // Send with retry mechanism
            bool sent = false;
            for (int attempt = 0; attempt < max_attempts && !sent; attempt++) {
                if (attempt > 0) {
                    log("Retrying send, attempt " + std::to_string(attempt + 1));
                }
                
                sent = communicator->send(msg);
                if (sent) break;
                
                // Brief backoff before retry
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            
            if (sent) {
                log("Message sent: " + msg.data());
                success_count++;
            } else {
                error("Failed to send message after " + std::to_string(max_attempts) + " attempts.");
                failure_count++;
            }
            
            // Receive message with timeout
            Message receivedMsg("");
            if (communicator->receive(receivedMsg)) {
                log("Message received: " + receivedMsg.data());
            } else {
                log("No message received within timeout.");
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(config.period_ms));
        }
        
        log("Communication complete. Success: " + std::to_string(success_count) + 
            ", Failures: " + std::to_string(failure_count));
    }
    
private:
    VehicleConfig config;
    int id;           // Vehicle identifier.
    NIC* nic = nullptr;         // NIC for this vehicle.
    Protocol* protocol = nullptr;       // Pointer to Protocol singleton.
    Communicator* communicator = nullptr; // Communicator for messaging.
    Protocol::Address communicator_address; // Address for this vehicle's communicator
};

// --------------------------
// Global variables for signal handling
// --------------------------
std::vector<pid_t> vehicle_pids;
volatile sig_atomic_t shutdown_requested = 0;

void signal_handler(int signal) {
    shutdown_requested = 1;
}

// --------------------------
// Main initializer script
// --------------------------
int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <number_of_vehicles> <message_periodicity_ms> [-v]" 
                  << std::endl;
        return EXIT_FAILURE;
    }
    
    int numVehicles = std::atoi(argv[1]);
    int period_ms = std::atoi(argv[2]);
    bool verbose = (argc > 3 && std::string(argv[3]) == "-v");
    
    std::cout << "Initializer: Creating " << numVehicles << " vehicle(s) with a message periodicity of "
              << period_ms << " ms." << std::endl;
    
    // Set up signal handling
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    
    // Fork a new process for each vehicle.
    for (int i = 0; i < numVehicles; ++i) {
        pid_t pid = fork();
        if (pid < 0) {
            std::cerr << "Error: Fork failed for vehicle " << i << std::endl;
            return EXIT_FAILURE;
        } else if (pid == 0) {
            // Child process
            // Reset signal handlers for child
            signal(SIGINT, SIG_DFL);
            signal(SIGTERM, SIG_DFL);
            
            std::cout << "Vehicle process " << i << " started (PID " << getpid() << ")." << std::endl;
            
            // Configure the vehicle
            VehicleConfig config;
            config.id = i;
            config.period_ms = period_ms;
            config.verbose_logging = verbose;
            config.log_prefix = "[PID " + std::to_string(getpid()) + "] ";
            
            try {
                Vehicle vehicle(config);
                vehicle.communicate();  // Run communication loop.
            } catch (const std::exception& e) {
                std::cerr << "Exception in vehicle " << i << ": " << e.what() << std::endl;
                exit(EXIT_FAILURE);
            }
            
            exit(EXIT_SUCCESS);
        } else {
            // Parent process
            vehicle_pids.push_back(pid);
        }
    }
    
    // Parent process waits and handles clean shutdown
    std::cout << "Initializer: All vehicles started. Press Ctrl+C to terminate." << std::endl;
    
    while (!shutdown_requested) {
        int status;
        pid_t finished = waitpid(-1, &status, WNOHANG);
        if (finished > 0) {
            auto it = std::find(vehicle_pids.begin(), vehicle_pids.end(), finished);
            if (it != vehicle_pids.end()) {
                vehicle_pids.erase(it);
                if (WIFEXITED(status)) {
                    std::cout << "Vehicle with PID " << finished 
                              << " exited with status " << WEXITSTATUS(status) << std::endl;
                } else if (WIFSIGNALED(status)) {
                    std::cout << "Vehicle with PID " << finished 
                              << " terminated by signal " << WTERMSIG(status) << std::endl;
                }
            }
        }
        
        if (vehicle_pids.empty()) {
            std::cout << "All vehicles have terminated." << std::endl;
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Clean shutdown: send termination signal to remaining vehicles
    if (!vehicle_pids.empty()) {
        std::cout << "Shutting down remaining vehicles..." << std::endl;
        for (pid_t pid : vehicle_pids) {
            kill(pid, SIGTERM);
        }
        
        // Wait for them to terminate
        for (pid_t pid : vehicle_pids) {
            int status;
            waitpid(pid, &status, 0);
            std::cout << "Vehicle with PID " << pid << " terminated." << std::endl;
        }
    }
    
    return EXIT_SUCCESS;
}