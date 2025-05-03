#include <iostream>
#include <pthread.h>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <random>
#include <chrono>
#include <sys/stat.h>
#include <fstream>
#include <filesystem>

#include "vehicle.h"
#include "debug.h"
#include "components/ecu_component.h"
#include "components/camera_component.h"
#include "components/lidar_component.h"
#include "components/ins_component.h"
#include "components/battery_component.h"
#include "test_utils.h"


void run_vehicle(Vehicle* v) {
    db<Vehicle>(TRC) << "run_vehicle() called!\n";

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist_lifetime(20, 40); // Lifetime from 60 to 90 seconds
    int lifetime = dist_lifetime(gen);
    unsigned int vehicle_id = v->id(); // Store ID before deletion

    db<Vehicle>(INF) << "[Vehicle " << v->id() << "] creating ECU1 component\n";
    v->create_component<ECUComponent>("ECU1", Vehicle::Ports::ECU1);

    db<Vehicle>(INF) << "[Vehicle " << v->id() << "] creating ECU2 component\n";
    v->create_component<ECUComponent>("ECU2", Vehicle::Ports::ECU2);

    db<Vehicle>(INF) << "[Vehicle " << v->id() << "] creating Lidar component\n";
    v->create_component<LidarComponent>("Lidar");

    db<Vehicle>(INF) << "[Vehicle " << v->id() << "] creating INS component\n";
    v->create_component<INSComponent>("INS");

    db<Vehicle>(INF) << "[Vehicle " << v->id() << "] creating Battery component\n";
    v->create_component<BatteryComponent>("Battery");

    v->start();
    db<Vehicle>(INF) << "[Vehicle " << v->id() << "] started.\n";

    // Wait for vehicle lifetime to end
    db<Vehicle>(INF) << "[Vehicle " << v->id() << "] sleeping for " << lifetime << "s\n";
    sleep(lifetime);
    db<Vehicle>(INF) << "[Vehicle " << v->id() << "] lifetime ended. Stopping vehicle.\n";

    try {
        // Signal the vehicle logic to stop
        v->stop(); // This now blocks until components are stopped and joined
        db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] vehicle stop() returned, proceeding to delete.\n";
        
        // Clean up vehicle (will delete components)
        delete v;
        
        db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] Vehicle object deleted and terminated cleanly.\n";
    } catch (const std::exception& e) {
        db<Vehicle>(ERR) << "[Vehicle " << vehicle_id << "] Exception during cleanup: " << e.what() << "\n";
    } catch (...) {
        db<Vehicle>(ERR) << "[Vehicle " << vehicle_id << "] Unknown error during cleanup\n";
    }
}

int main(int argc, char* argv[]) {
    TEST_INIT("system_demo");
    
    TEST_LOG("Application started!");

    unsigned int n_vehicles = 100;

    // Create logs directory if it doesn't exist
    std::filesystem::create_directory("logs");
    std::filesystem::permissions("logs",  std::filesystem::perms::others_all);

    std::vector<pid_t> children;

    for (unsigned int id = 1; id <= n_vehicles; ++id) {
        pid_t pid = fork();

        if (pid < 0) {
            TEST_LOG("[ERROR] failed to fork process");
            TEST_LOG("Application terminated.");
            return -1;
        }

        if (pid == 0) {
            std::string vehicle_dir = "logs/vehicle_" + std::to_string(id);
            std::filesystem::create_directory(vehicle_dir);
            std::filesystem::permissions(vehicle_dir,  std::filesystem::perms::others_all);
            
            std::string log_file = "./logs/vehicle_" + std::to_string(id) + "/vehicle_" + std::to_string(id) + ".log";
            Debug::set_log_file(log_file);

            std::string log_message = "[Child " + std::to_string(getpid()) + "] creating vehicle " + std::to_string(id);
            
            // Child processes don't share logger, so we still need to use cout here
            std::cout << log_message << std::endl;
            
            Vehicle* v = new Vehicle(id);
            run_vehicle(v);
            
            Debug::close_log_file();
            
            log_message = "[Child " + std::to_string(getpid()) + "] vehicle " + std::to_string(id) + " finished execution";
            std::cout << log_message << std::endl;

            exit(0);
        } else {
            children.push_back(pid);
            TEST_LOG("Created child process " + std::to_string(pid) + " for vehicle " + std::to_string(id));
        }
    }

    bool successful = true;

    for (pid_t child_pid : children) {
        int status;
        if (waitpid(child_pid, &status, 0) == -1) {
            TEST_LOG("[ERROR] failed to wait for child " + std::to_string(child_pid));
            TEST_LOG("Application terminated.");
            return -1;
        } else {
            // Improved process status reporting
            if (WIFEXITED(status)) {
                int exit_status = WEXITSTATUS(status);
                TEST_LOG("[Parent] child " + std::to_string(child_pid) + " exited normally with status " + std::to_string(exit_status));
                if (exit_status != 0) {
                    successful = false;
                }
            } else if (WIFSIGNALED(status)) {
                int signal = WTERMSIG(status);
                TEST_LOG("[Parent] child " + std::to_string(child_pid) + " terminated by signal " + std::to_string(signal));
                successful = false;
            } else {
                TEST_LOG("[Parent] child " + std::to_string(child_pid) + " terminated with unknown status " + std::to_string(status));
                successful = false;
            }
        }
    }

    if (!successful) {
        TEST_LOG("Application terminated with errors!");
        return -1;
    }

    TEST_LOG("Application completed successfully!");
    return 0;
}