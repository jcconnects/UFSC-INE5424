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

#include "initializer.h"
#include "vehicle.h"
#include "debug.h"
#include "component.h"
#include "components/sender_component.h"
#include "components/receiver_component.h"
#include "test_utils.h"

// Helper function to get the test interface name
std::string get_test_interface() {
    std::string interface_name = "test-dummy0"; // Default
    std::ifstream iface_file("tests/logs/current_test_iface");
    if (iface_file) {
        std::getline(iface_file, interface_name);
        iface_file.close();
    }
    return interface_name;
}

void run_vehicle(Vehicle* v, std::string log_prefix) {
    db<Vehicle>(TRC) << "run_vehicle() called!\n";

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist_lifetime(5, 20);
    int lifetime = dist_lifetime(gen); // Reduced lifetime range from 10-50 seconds

    // Create components based on vehicle ID
    // Even ID vehicles will send and receive
    // Odd ID vehicles will only receive
    if (v->id() % 2 == 0) {
        db<Vehicle>(INF) << "[Vehicle " << v->id() << "] creating sender component\n";
        v->add_component(new SenderComponent(v));
    }
    
    db<Vehicle>(INF) << "[Vehicle " << v->id() << "] creating receiver component\n";
    v->add_component(new ReceiverComponent(v));

    v->start();
    db<Vehicle>(INF) << "[Vehicle " << v->id() << "] starting. Lifetime: " << lifetime << "s\n";

    // Wait for vehicle lifetime to end
    db<Vehicle>(INF) << "[Vehicle " << v->id() << "] sleeping for lifetime: " << lifetime << "s\n";
    sleep(lifetime);
    db<Vehicle>(INF) << "[Vehicle " << v->id() << "] lifetime ended. Stopping vehicle.\n";

    try {
        // Signal the vehicle logic to stop
        v->stop();
        db<Vehicle>(INF) << "[Vehicle " << v->id() << "] stopped, about to delete\n";
        
        // Clean up vehicle (will delete components)
        delete v;
        
        db<Vehicle>(INF) << "Vehicle deleted and terminated cleanly.\n";
    } catch (const std::exception& e) {
        db<Vehicle>(ERR) << "[Vehicle " << v->id() << "] Exception during cleanup: " << e.what() << "\n";
    } catch (...) {
        db<Vehicle>(ERR) << "[Vehicle " << v->id() << "] Unknown error during cleanup\n";
    }
}

int main(int argc, char* argv[]) {
    TEST_INIT("system_demo");
    
    TEST_LOG("Application started!");

    unsigned int n_vehicles = 100;

    // Create logs directory if it doesn't exist
    mkdir("./logs", 0777);

    std::vector<pid_t> children;

    for (unsigned int id = 1; id <= n_vehicles; ++id) {
        pid_t pid = fork();

        if (pid < 0) {
            TEST_LOG("[ERROR] failed to fork process");
            TEST_LOG("Application terminated.");
            return -1;
        }

        if (pid == 0) {
            std::string log_file = "./logs/vehicle_" + std::to_string(id) + ".log";
            Debug::set_log_file(log_file);

            std::string log_message = "[Child " + std::to_string(getpid()) + "] creating vehicle " + std::to_string(id);
            // Child processes don't share logger, so we still need to use cout here
            std::cout << log_message << std::endl;
            
            Vehicle* v = Initializer::create_vehicle(id);
            run_vehicle(v, "Vehicle_" + std::to_string(id));
            
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
            TEST_LOG("[Parent] child " + std::to_string(child_pid) + " terminated with status " + std::to_string(status));
            if (status != 0) {
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