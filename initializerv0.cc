#include <iostream>
#include <cstdlib>
#include <unistd.h>     // for fork(), getpid()
#include <sys/wait.h>   // for wait()
#include <string>
#include <chrono>       // for std::chrono::milliseconds
#include <thread>       // for std::this_thread::sleep_for

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
        std::cout << "[NIC] NIC initialized in process " << getpid() << std::endl;
    }
    ~NIC() {}
};

// Protocol implements the communication protocol as a singleton.
class Protocol {
public:
    static Protocol& getInstance() {
        static Protocol instance;  // One instance per process.
        return instance;
    }
    
    int send(const Message & msg) {
        std::cout << "[Protocol] (PID " << getpid() << ") Sending message: " << msg.data() << std::endl;
        return 1; // Success simulation.
    }
    
    int receive(Message & msg) {
        msg = Message("Dummy received message");
        return static_cast<int>(msg.size());
    }
    
    // Methods for attaching/detaching observers would be here.
    
private:
    Protocol() {
        std::cout << "[Protocol] Protocol singleton instantiated in process " << getpid() << std::endl;
    }
    Protocol(const Protocol&) = delete;
    Protocol& operator=(const Protocol&) = delete;
};

// Communicator uses Protocol to send and receive messages.
class Communicator {
public:
    Communicator() {
        std::cout << "[Communicator] Communicator created in process " << getpid() << std::endl;
        // In the real implementation, this constructor attaches the Communicator as an observer to Protocol.
    }
    ~Communicator() {}
    
    bool send(const Message & msg) {
        return Protocol::getInstance().send(msg) > 0;
    }
    
    bool receive(Message & msg) {
        return Protocol::getInstance().receive(msg) > 0;
    }
};

// --------------------------
// Vehicle class definition
// --------------------------
// The Vehicle class now encapsulates the instantiation of NIC, Protocol, and Communicator,
// and handles all attachment/detachment of the communication pipeline.
class Vehicle {
public:
    Vehicle(int id, int period_ms)
        : id(id), period_ms(period_ms)
    {
        setupCommunicationPipeline();
    }
    
    ~Vehicle() {
        teardownCommunicationPipeline();
    }
    
    // Sets up the communication pipeline in a black box fashion.
    void setupCommunicationPipeline() {
        // Create NIC instance.
        nic = new NIC();
        // Get the Protocol singleton (this may internally attach itself to NIC).
        protocol = &Protocol::getInstance();
        // Create a Communicator and attach it as an observer to the Protocol.
        communicator = new Communicator();
        std::cout << "[Vehicle " << id << "] Communication pipeline set up." << std::endl;
    }
    
    // Tears down the communication pipeline.
    void teardownCommunicationPipeline() {
        // In a complete implementation, detach observers and perform clean-up.
        delete communicator;
        delete nic;
        std::cout << "[Vehicle " << id << "] Communication pipeline torn down." << std::endl;
    }
    
    // Simulates periodic communication.
    void communicate() {
        int counter = 0;
        while (counter++ < 10) {
            // Create a message that includes the vehicle id.
            std::string msgContent = "Vehicle " + std::to_string(id) + " reporting in.";
            Message msg(msgContent);
            
            // Send the message.
            if (communicator->send(msg)) {
                std::cout << "[Vehicle " << id << "] Message sent: " << msg.data() << std::endl;
            } else {
                std::cerr << "[Vehicle " << id << "] Failed to send message." << std::endl;
            }
            
            // Simulate message reception.
            Message receivedMsg("");
            if (communicator->receive(receivedMsg)) {
                std::cout << "[Vehicle " << id << "] Message received: " << receivedMsg.data() << std::endl;
            }
            
            // Sleep for the specified period.
            std::this_thread::sleep_for(std::chrono::milliseconds(period_ms));
        }
    }
    
private:
    int id;           // Vehicle identifier.
    int period_ms;    // Message periodicity in milliseconds.
    NIC* nic;         // NIC for this vehicle.
    Protocol* protocol;       // Pointer to Protocol singleton.
    Communicator* communicator; // Communicator for messaging.
};

// --------------------------
// Main initializer script
// --------------------------
int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <number_of_vehicles> <message_periodicity_ms>" << std::endl;
        return EXIT_FAILURE;
    }
    
    int numVehicles = std::atoi(argv[1]);
    int period_ms = std::atoi(argv[2]);
    
    std::cout << "Initializer: Creating " << numVehicles << " vehicle(s) with a message periodicity of "
              << period_ms << " ms." << std::endl;
    
    // Fork a new process for each vehicle.
    for (int i = 0; i < numVehicles; ++i) {
        pid_t pid = fork();
        if (pid < 0) {
            std::cerr << "Error: Fork failed for vehicle " << i << std::endl;
            return EXIT_FAILURE;
        } else if (pid == 0) {
            std::cout << "Vehicle process " << i << " started (PID " << getpid() << ")." << std::endl;
            Vehicle vehicle(i, period_ms);
            vehicle.communicate();  // Run communication loop.
            exit(EXIT_SUCCESS);
        }
        // Parent process continues forking.
    }
    
    // Parent process waits for child processes.
    while (true) {
        int status;
        pid_t finished = wait(&status);
        if (finished == -1) {
            break;
        }
    }
    
    return EXIT_SUCCESS;
}
