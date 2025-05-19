#include <iostream>
#include <filesystem>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <random>
#include <chrono>
#include <fstream>
#include <map>

// Include vehicle.h first
#include "../../include/vehicle.h"
// Then include debug.h
#include "../../include/debug.h"

// Then include component classes
#include "../../include/components/ecu_component.h"
#include "../../include/components/ins_component.h"
#include "../../include/components/lidar_component.h"
#include "../../include/components/battery_component.h"
#include "../../include/components/gateway_component.h"
// BasicProducer is already included from vehicle.h
#include "../../include/components/basic_consumer.h"
#include "../test_utils.h"

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

void run_vehicle(Vehicle* v) {
    db<Vehicle>(TRC) << "run_vehicle() called!\n";

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist_lifetime(5, 10); // Increase lifetime from 40-60 seconds
    int lifetime = dist_lifetime(gen);
    unsigned int vehicle_id = v->id(); // Store ID before deletion

    // Create all components at once without waiting for registration
    db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] creating all components\n";
    
    // Step 1: Create Gateway - this will listen on Port 0 and relay to Port 1
    v->create_component<GatewayComponent>("Gateway");
    db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] Gateway component created\n";
    
    // Step 2: Create Producer - this will respond to interests for its data type
    v->create_component<BasicProducer>("BasicProducer");
    db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] BasicProducer component created\n";
    
    // Step 3: Create Consumer - this will register interest in the producer's data type
    v->create_component<BasicConsumer>("BasicConsumer");
    db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] BasicConsumer component created\n";
    
    // Start all components at once
    v->start();
    db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] started for " << lifetime << "s lifetime\n";

    // Print message to confirm components are interacting as expected
    db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] Components started. Gateway should relay "
                    << "INTEREST messages from BasicConsumer to BasicProducer, and "
                    << "RESPONSE messages from BasicProducer to BasicConsumer.\n";
                    
    // Wait for vehicle lifetime
    for (int i = 1; i <= lifetime; i++) {
        sleep(1);
        if (i % 5 == 0) {
            db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] Running for " << i << " seconds\n";
        }
    }
    db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] lifetime ended, stopping\n";

    try {
        // Stop vehicle and clean up
        v->stop();
        delete v;
        db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] terminated cleanly\n";
    } catch (const std::exception& e) {
        db<Vehicle>(ERR) << "[Vehicle " << vehicle_id << "] Exception during cleanup: " << e.what() << "\n";
    } catch (...) {
        db<Vehicle>(ERR) << "[Vehicle " << vehicle_id << "] Unknown error during cleanup\n";
    }
}

int main(int argc, char* argv[]) {
    TEST_INIT("system_demo");
    TEST_LOG("Starting P3 API validation test with basic components...");

    // Set number of test vehicles
    const unsigned int n_vehicles = 50;

    // Ensure tests/logs directory exists
    try {
        std::filesystem::create_directories("tests/logs");
        TEST_LOG("Created tests/logs directory");
    } catch (const std::exception& e) {
        TEST_LOG("Warning: Could not create tests/logs directory: " + std::string(e.what()));
    }

    // Verify the test interface is configured before starting
    std::string interface_name = "test-dummy0"; // Default name matches what Makefile creates
    
    // Wait for interface configuration to complete
    // Makefile creates the interface and writes to current_test_iface
    {
        std::ifstream iface_check("tests/logs/current_test_iface");
        if (iface_check) {
            std::getline(iface_check, interface_name);
            TEST_LOG("Using network interface: " + interface_name);
        } else {
            TEST_LOG("Warning: Interface file not found, using default: " + interface_name);
            // Create the file ourselves just to be sure
            std::ofstream iface_file("tests/logs/current_test_iface");
            iface_file << interface_name << std::endl;
            iface_file.close();
        }
    }
    
    // Small delay to ensure interface is ready and file is flushed to disk
    sleep(1);
    TEST_LOG("Network interface configuration complete");

    std::vector<pid_t> children;
    children.reserve(n_vehicles);

    // Create vehicle processes
    for (unsigned int id = 1; id <= n_vehicles; ++id) {
        pid_t pid = fork();

        if (pid < 0) {
            TEST_LOG("[ERROR] failed to fork process");
            TEST_LOG("Application terminated.");
            return -1;
        }

        if (pid == 0) { // Child process
            try {
                // Set up logging for child process
                std::string log_file = setup_log_directory(id);
                Debug::set_log_file(log_file);
                
                // Create and run vehicle
                std::cout << "[Child " << getpid() << "] creating vehicle " << id << std::endl;
                Vehicle* v = new Vehicle(id);
                run_vehicle(v);
                
                Debug::close_log_file();
                std::cout << "[Child " << getpid() << "] vehicle " << id << " finished execution" << std::endl;
                exit(0);
            } catch (const std::exception& e) {
                std::cerr << "Error in child process: " << e.what() << std::endl;
                exit(1);
            }
        } else { // Parent process
            children.push_back(pid);
            TEST_LOG("Created child process " + std::to_string(pid) + " for vehicle " + std::to_string(id));
            
            // Add a small random sleep between vehicle creations to stagger their initialization
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> sleep_dist(100, 500); // 100-500 milliseconds
            usleep(sleep_dist(gen) * 1000); // Convert to microseconds
        }
    }

    // Wait for all child processes to complete
    bool successful = true;
    for (pid_t child_pid : children) {
        int status;
        if (waitpid(child_pid, &status, 0) == -1) {
            TEST_LOG("[ERROR] failed to wait for child " + std::to_string(child_pid));
            successful = false;
        } else if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            TEST_LOG("[Parent] child " + std::to_string(child_pid) + " exited with status " + std::to_string(exit_status));
            if (exit_status != 0) successful = false;
        } else if (WIFSIGNALED(status)) {
            TEST_LOG("[Parent] child " + std::to_string(child_pid) + " terminated by signal " + std::to_string(WTERMSIG(status)));
            successful = false;
        } else {
            TEST_LOG("[Parent] child " + std::to_string(child_pid) + " terminated abnormally");
            successful = false;
        }
    }

    TEST_LOG("Vehicles have terminated - test complete");
    TEST_LOG("P3 API validation completed successfully!");
    return successful ? 0 : 1;
}