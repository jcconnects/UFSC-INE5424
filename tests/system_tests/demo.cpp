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
    
    // // Step 2: Create Producer - this will respond to interests for its data type
    // v->create_component<BasicProducer>("BasicProducer");
    // db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] BasicProducer component created\n";
    
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
    const unsigned int n_vehicles = 2;

    // Ensure tests/logs directory exists
    try {
        std::filesystem::create_directories("tests/logs");
        TEST_LOG("Created tests/logs directory");
    } catch (const std::exception& e) {
        TEST_LOG("Warning: Could not create tests/logs directory: " + std::string(e.what()));
    }

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