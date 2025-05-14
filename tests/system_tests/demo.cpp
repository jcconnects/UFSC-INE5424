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
#include "vehicle.h"
// Then include debug.h
#include "debug.h"

// Then include component classes
#include "components/ecu_component.h"
#include "components/ins_component.h"
#include "components/lidar_component.h"
#include "components/battery_component.h"
#include "components/gateway_component.h"
// BasicProducer is already included from vehicle.h
#include "components/basic_consumer.h"
#include "test_utils.h"

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

// Helper function to verify logs contain expected P3 messages
bool verify_logs(unsigned int num_vehicles) {
    db<Vehicle>(INF) << "Verifying logs for P3 functionality...\n";
    bool success = true;
    
    // Keywords to search for in logs that indicate P3 functionality
    std::vector<std::string> p3_keywords = {
        "sent REG_PRODUCER",
        "received INTEREST",
        "sending RESPONSE",
        "received RESPONSE",
        "relaying INTEREST"
    };
    
    for (unsigned int id = 1; id <= num_vehicles; id++) {
        std::string log_path = "tests/logs/vehicle_" + std::to_string(id) + "/vehicle_" + std::to_string(id) + ".log";
        std::ifstream log_file(log_path);
        
        if (!log_file.is_open()) {
            db<Vehicle>(ERR) << "Failed to open log file: " << log_path << "\n";
            success = false;
            continue;
        }
        
        // Track which P3 functionalities were observed
        std::map<std::string, bool> observed;
        for (const auto& keyword : p3_keywords) {
            observed[keyword] = false;
        }
        
        std::string line;
        while (std::getline(log_file, line)) {
            for (const auto& keyword : p3_keywords) {
                if (line.find(keyword) != std::string::npos) {
                    observed[keyword] = true;
                }
            }
        }
        
        // Report missing functionality
        for (const auto& [keyword, found] : observed) {
            if (!found) {
                db<Vehicle>(ERR) << "Vehicle " << id << " logs missing evidence of: " << keyword << "\n";
                success = false;
            }
        }
    }
    
    return success;
}

void run_vehicle(Vehicle* v) {
    db<Vehicle>(TRC) << "run_vehicle() called!\n";

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist_lifetime(20, 40); // Lifetime from 20 to 40 seconds
    int lifetime = dist_lifetime(gen);
    unsigned int vehicle_id = v->id(); // Store ID before deletion

    // Create all components at once without waiting for registration
    db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] creating all components\n";
    
    // Step 1: Create Gateway
    v->create_component<GatewayComponent>("Gateway");
    db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] Gateway component created\n";
    
    // Step 2: Create Producer
    v->create_component<BasicProducer>("BasicProducer");
    db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] BasicProducer component created\n";
    
    // Step 3: Create Consumer
    v->create_component<BasicConsumer>("BasicConsumer");
    db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] BasicConsumer component created\n";
    
    // Start all components at once
    v->start();
    db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] started for " << lifetime << "s lifetime\n";

    // Wait for vehicle lifetime
    sleep(lifetime);
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

    // Verify test results by analyzing logs
    TEST_LOG("Vehicles have terminated, analyzing logs to verify P3 functionality...");
    bool logs_valid = verify_logs(n_vehicles);
    
    if (!logs_valid) {
        TEST_LOG("WARNING: Log verification found missing P3 operations!");
        successful = false;
    } else {
        TEST_LOG("All P3 API functions successfully observed in logs!");
    }

    TEST_LOG(successful ? "P3 API validation completed successfully!" : "P3 API validation test FAILED!");
    return successful ? 0 : -1;
}