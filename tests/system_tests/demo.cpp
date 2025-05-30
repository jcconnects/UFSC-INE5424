#include <iostream>
#include <filesystem>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <random>
#include <chrono>
#include <signal.h>
#include <errno.h>

#include "../../include/app/vehicle.h"
#include "../../include/api/util/debug.h"
#include "../../include/app/components/basic_producer_a.h"
#include "../../include/app/components/basic_consumer_a.h"
#include "../../include/app/components/basic_producer_b.h"
#include "../../include/app/components/basic_consumer_b.h"
#include "../../include/api/framework/rsu.h"
#include "../../include/api/framework/leaderKeyStorage.h"
#include "../testcase.h"
#include "../test_utils.h"

// Global flag for RSU termination - volatile for signal safety
volatile sig_atomic_t rsu_should_terminate = 0;

// Signal handler for RSU termination
void rsu_signal_handler(int signal) {
    if (signal == SIGUSR1) {
        rsu_should_terminate = 1;
        // Write to stderr for immediate output (safer in signal handler)
        write(STDERR_FILENO, "[RSU Signal] Received SIGUSR1, setting termination flag\n", 56);
    }
}

// Helper function to set up a vehicle log directory
std::string setup_log_directory(unsigned int vehicle_id) {
    std::string log_file;
    std::error_code ec;
    
    // Try in priority order: Docker logs dir, tests/logs dir, current dir
    if (std::filesystem::exists("/app/logs")) {
        std::string vehicle_dir = "/app/logs/vehicle_" + std::to_string(vehicle_id);
        std::filesystem::create_directory(vehicle_dir, ec);
        
        if (!ec) {
            return vehicle_dir + "/vehicle_" + std::to_string(vehicle_id) + ".log";
        }
    }
    
    // Try tests/logs directory
    std::string test_logs_dir = "tests/logs/vehicle_" + std::to_string(vehicle_id);
    
    try {
        std::filesystem::create_directories(test_logs_dir);
        return test_logs_dir + "/vehicle_" + std::to_string(vehicle_id) + ".log";
    } catch (...) {
        // Fallback to tests/logs without vehicle subfolder
        return "tests/logs/vehicle_" + std::to_string(vehicle_id) + ".log";
    }
}

// Helper function to set up RSU log directory
std::string setup_rsu_log_directory(unsigned int rsu_id) {
    std::string log_file;
    std::error_code ec;
    
    // Try in priority order: Docker logs dir, tests/logs dir, current dir
    if (std::filesystem::exists("/app/logs")) {
        std::string rsu_dir = "/app/logs/rsu_" + std::to_string(rsu_id);
        std::filesystem::create_directory(rsu_dir, ec);
        
        if (!ec) {
            return rsu_dir + "/rsu_" + std::to_string(rsu_id) + ".log";
        }
    }
    
    // Try tests/logs directory
    std::string test_logs_dir = "tests/logs/rsu_" + std::to_string(rsu_id);
    
    try {
        std::filesystem::create_directories(test_logs_dir);
        return test_logs_dir + "/rsu_" + std::to_string(rsu_id) + ".log";
    } catch (...) {
        // Fallback to tests/logs without RSU subfolder
        return "tests/logs/rsu_" + std::to_string(rsu_id) + ".log";
    }
}

class Demo: public TestCase {
    public:
        Demo();
        ~Demo() = default;

        void setUp() override;
        void tearDown() override;

        /* TESTS */
        int run_demo();
    private:
        void run_vehicle(Vehicle* v);
        void run_rsu(unsigned int rsu_id, unsigned int unit, std::chrono::milliseconds period);
        void setup_rsu_as_leader(unsigned int rsu_id);
};

Demo::Demo() {
    TEST_INIT("system_demo");
    TEST_LOG("Application started!");
    DEFINE_TEST(run_demo);
}

void Demo::setUp() {
    // Set up code if needed
}

void Demo::tearDown() {
    // Tear down code if needed
    TEST_INIT("test teardown");
    TEST_LOG("Demo test case completed.");
}

void Demo::setup_rsu_as_leader(unsigned int rsu_id) {
    TEST_INIT("Setting up RSU as leader");
    // Create a high-age unique key for the RSU to ensure it becomes leader
    MacKeyType rsu_key;
    rsu_key.fill(0);
    // Use RSU ID in the key to make it unique
    rsu_key[0] = (rsu_id >> 8) & 0xFF;
    rsu_key[1] = rsu_id & 0xFF;
    rsu_key[2] = 0xAA; // Marker for RSU
    rsu_key[3] = 0xBB;
    
    // Create RSU MAC address (similar to how Network creates vehicle MACs)
    Ethernet::Address rsu_mac;
    rsu_mac.bytes[0] = 0x02; // Locally administered
    rsu_mac.bytes[1] = 0x00;
    rsu_mac.bytes[2] = 0x00;
    rsu_mac.bytes[3] = 0x00;
    rsu_mac.bytes[4] = (rsu_id >> 8) & 0xFF;
    rsu_mac.bytes[5] = rsu_id & 0xFF;
    
    // Set RSU as leader in LeaderKeyStorage
    auto& storage = LeaderKeyStorage::getInstance();
    storage.setLeaderId(rsu_mac);
    storage.setGroupMacKey(rsu_key);
    
    TEST_LOG("RSU " + std::to_string(rsu_id) + " set as leader with MAC: " + 
             Ethernet::mac_to_string(rsu_mac));
}

void Demo::run_rsu(unsigned int rsu_id, unsigned int unit, std::chrono::milliseconds period) {
    try {
        // Set up signal handler for graceful termination
        signal(SIGUSR1, rsu_signal_handler);
        
        // Set up logging for RSU process
        std::string log_file = setup_rsu_log_directory(rsu_id);
        Debug::set_log_file(log_file);
        
        db<RSU>(INF) << "[RSU " << rsu_id << "] Starting RSU process\n";
        
        // Setup RSU as leader before creating the RSU instance
        setup_rsu_as_leader(rsu_id);
        
        // Create RSU with specified parameters
        // Unit 999 is a special unit for RSU broadcasts
        // Period of 500ms for regular status broadcasts
        RSU* rsu = new RSU(rsu_id, unit, period);
        
        db<RSU>(INF) << "[RSU " << rsu_id << "] RSU created with address " 
                     << rsu->address().to_string() << "\n";
        
        // Start the RSU
        rsu->start();
        db<RSU>(INF) << "[RSU " << rsu_id << "] RSU started, broadcasting every " 
                     << period.count() << "ms\n";
        
        // Wait for signal to stop instead of fixed duration
        db<RSU>(INF) << "[RSU " << rsu_id << "] Waiting for all vehicles to complete...\n";
        while (!rsu_should_terminate) {
            pause(); // Wait for signal - more responsive than sleep(1)
        }
        
        db<RSU>(INF) << "[RSU " << rsu_id << "] Received termination signal, stopping RSU\n";
        rsu->stop();
        delete rsu;
        
        Debug::close_log_file();
        db<RSU>(INF) << "[RSU " << rsu_id << "] RSU process finished\n";
        
    } catch (const std::exception& e) {
        std::cerr << "[RSU " << rsu_id << "] Error in RSU process: " << e.what() << std::endl;
        exit(1);
    }
}

int Demo::run_demo() {
    TEST_INIT("the demo test case");
    
    // RSU Configuration
    const unsigned int RSU_ID = 1000; // High ID to distinguish from vehicles
    const unsigned int RSU_UNIT = 999; // Special unit for RSU
    const auto RSU_PERIOD = std::chrono::milliseconds(250); // 250ms broadcast period
    
    // Vehicle Configuration
    const unsigned int n_vehicles = 30;

    // Ensure tests/logs directory exists
    try {
        std::filesystem::create_directories("tests/logs");
        TEST_LOG("Created tests/logs directory");
    } catch (const std::exception& e) {
        TEST_LOG("Warning: Could not create tests/logs directory: " + std::string(e.what()));
    }

    std::vector<pid_t> children;
    children.reserve(n_vehicles + 1); // +1 for RSU

    // === STEP 1: Create and start RSU process first ===
    TEST_LOG("Creating RSU process (ID: " + std::to_string(RSU_ID) + ")");
    
    pid_t rsu_pid = fork();
    if (rsu_pid < 0) {
        TEST_LOG("[ERROR] failed to fork RSU process");
        return -1;
    }
    
    if (rsu_pid == 0) { // RSU child process
        std::cout << "[RSU Child " << getpid() << "] creating RSU " << RSU_ID << std::endl;
        run_rsu(RSU_ID, RSU_UNIT, RSU_PERIOD);
        exit(0);
    } else { // Parent process
        children.push_back(rsu_pid);
        TEST_LOG("Created RSU process " + std::to_string(rsu_pid) + " for RSU " + std::to_string(RSU_ID));
        
        // Give RSU time to start and establish leadership
        sleep(2);
        TEST_LOG("RSU startup delay completed, proceeding with vehicle creation");
    }

    // === STEP 2: Create vehicle processes ===
    TEST_LOG("Creating " + std::to_string(n_vehicles) + " vehicle processes");
    
    for (unsigned int id = 1; id <= n_vehicles; ++id) {
        // Sleep for 500ms between vehicle creations to stagger startup
        usleep(500000);

        pid_t pid = fork();

        if (pid < 0) {
            TEST_LOG("[ERROR] failed to fork vehicle process for ID " + std::to_string(id));
            TEST_LOG("Application terminated.");
            return -1;
        }

        if (pid == 0) { // Vehicle child process
            try {
                // Set up logging for child process
                std::string log_file = setup_log_directory(id);
                Debug::set_log_file(log_file);
                
                // Create and run vehicle
                std::cout << "[Vehicle Child " << getpid() << "] creating vehicle " << id << std::endl;
                Vehicle* v = new Vehicle(id);
                run_vehicle(v);
                
                Debug::close_log_file();
                std::cout << "[Vehicle Child " << getpid() << "] vehicle " << id << " finished execution" << std::endl;
                exit(0);
            } catch (const std::exception& e) {
                std::cerr << "Error in vehicle child process: " << e.what() << std::endl;
                exit(1);
            }
        } else { // Parent process
            children.push_back(pid);
            TEST_LOG("Created vehicle child process " + std::to_string(pid) + " for vehicle " + std::to_string(id));
        }
    }

    // === STEP 3: Wait for all vehicle processes to complete first ===
    TEST_LOG("Waiting for all " + std::to_string(n_vehicles) + " vehicle processes to complete");
    
    bool successful = true;
    int completed_vehicles = 0;
    
    // Wait for all vehicle processes (excluding RSU)
    for (size_t i = 1; i < children.size(); ++i) { // Skip RSU (index 0)
        pid_t child_pid = children[i];
        int status;
        if (waitpid(child_pid, &status, 0) == -1) {
            TEST_LOG("[ERROR] failed to wait for vehicle child " + std::to_string(child_pid));
            successful = false;
        } else if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            completed_vehicles++;
            TEST_LOG("[Parent] Vehicle child " + std::to_string(child_pid) + " exited with status " + std::to_string(exit_status));
            if (exit_status != 0) successful = false;
        } else if (WIFSIGNALED(status)) {
            TEST_LOG("[Parent] Vehicle child " + std::to_string(child_pid) + " terminated by signal " + std::to_string(WTERMSIG(status)));
            successful = false;
        } else {
            TEST_LOG("[Parent] Vehicle child " + std::to_string(child_pid) + " terminated abnormally");
            successful = false;
        }
    }

    TEST_LOG("All vehicles completed: " + std::to_string(completed_vehicles) + "/" + std::to_string(n_vehicles) + " vehicles finished");
    
    // === STEP 4: Signal RSU to terminate ===
    TEST_LOG("Signaling RSU to terminate");
    if (kill(rsu_pid, SIGUSR1) == -1) {
        TEST_LOG("[ERROR] Failed to signal RSU process: " + std::string(strerror(errno)));
        successful = false;
    } else {
        TEST_LOG("Successfully signaled RSU to terminate");
    }
    
    // === STEP 5: Wait for RSU to complete ===
    TEST_LOG("Waiting for RSU process to complete");
    int status;
    // Add a timeout in case RSU doesn't respond - wait up to 10 seconds
    pid_t wait_result = waitpid(rsu_pid, &status, WNOHANG);
    int timeout_counter = 0;
    while (wait_result == 0 && timeout_counter < 10) {
        sleep(1);
        timeout_counter++;
        TEST_LOG("Waiting for RSU... (" + std::to_string(timeout_counter) + "/10 seconds)");
        wait_result = waitpid(rsu_pid, &status, WNOHANG);
    }
    
    if (wait_result == -1) {
        TEST_LOG("[ERROR] failed to wait for RSU process " + std::to_string(rsu_pid) + ": " + std::string(strerror(errno)));
        successful = false;
    } else if (wait_result == 0) {
        TEST_LOG("[ERROR] RSU process " + std::to_string(rsu_pid) + " did not terminate within timeout, killing it");
        kill(rsu_pid, SIGKILL);
        waitpid(rsu_pid, &status, 0);
        successful = false;
    } else if (WIFEXITED(status)) {
        int exit_status = WEXITSTATUS(status);
        TEST_LOG("[Parent] RSU process " + std::to_string(rsu_pid) + " exited with status " + std::to_string(exit_status));
        if (exit_status != 0) successful = false;
    } else if (WIFSIGNALED(status)) {
        TEST_LOG("[Parent] RSU process " + std::to_string(rsu_pid) + " terminated by signal " + std::to_string(WTERMSIG(status)));
        successful = false;
    } else {
        TEST_LOG("[Parent] RSU process " + std::to_string(rsu_pid) + " terminated abnormally");
        successful = false;
    }

    TEST_LOG("Demo completed: " + std::to_string(completed_vehicles) + " vehicles and 1 RSU finished");
    TEST_LOG(successful ? "Application completed successfully!" : "Application terminated with errors!");
    
    return successful ? 0 : -1;
}

void Demo::run_vehicle(Vehicle* v) {
    db<Vehicle>(TRC) << "run_vehicle() called!\n";

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist_lifetime(3, 7); // Lifetime from 3 to 7 seconds
    std::uniform_int_distribution<> start_delay(0, 3);
    int delay = start_delay(gen);
    int lifetime = dist_lifetime(gen);
    unsigned int vehicle_id = v->id(); // Store ID before deletion

    // Create simple components based on vehicle ID
    db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] creating components\n";
    
    // Even ID vehicles produce A and consume B
    // Odd ID vehicles produce B and consume A
    if (vehicle_id % 2 == 0) {
        v->create_component<BasicProducerA>("ProducerA");
        v->create_component<BasicConsumerB>("ConsumerB");
    } else {
        v->create_component<BasicProducerB>("ProducerB");
        v->create_component<BasicConsumerA>("ConsumerA");
    }

    db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] components created, starting in " << delay << "s\n";

    sleep(delay); // Simulate startup delay

    db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] starting vehicle after " << delay << "s delay\n";

    // Start the vehicle
    v->start();
    db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] started for " << lifetime << "s lifetime\n";

    // Wait for vehicle lifetime
    sleep(lifetime);
    db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] lifetime ended, stopping\n";

    try {
        // Stop vehicle and clean up
        v->stop();
        db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] stopped, cleaning up\n";
        delete v;
        db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] terminated cleanly\n";
    } catch (const std::exception& e) {
        db<Vehicle>(ERR) << "[Vehicle " << vehicle_id << "] Exception during cleanup: " << e.what() << "\n";
    } catch (...) {
        db<Vehicle>(ERR) << "[Vehicle " << vehicle_id << "] Unknown error during cleanup\n";
    }
}

int main(int argc, char* argv[]) {
    Demo demo;
    demo.run();

 return 0;
}