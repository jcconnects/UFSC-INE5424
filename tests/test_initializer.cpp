#include "../include/initializer.h"
#include "../include/vehicle.h"
#include <iostream>
#include <vector>
#include <csignal>
#include <thread>
#include <chrono>

volatile sig_atomic_t shutdown_requested = 0;

void signal_handler(int signal) {
    shutdown_requested = 1;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <number_of_vehicles> <message_periodicity_ms> <timeout_seconds> [-v]" 
                  << std::endl;
        return EXIT_FAILURE;
    }
    
    int numVehicles = std::atoi(argv[1]);
    int period_ms = std::atoi(argv[2]);
    int timeout_seconds = std::atoi(argv[3]);
    
    // Set up signal handling
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    
    std::cout << "Creating " << numVehicles << " vehicles with message periodicity of "
              << period_ms << " ms and timeout of " << timeout_seconds << " seconds." << std::endl;
    
    // Create and start initializers
    std::vector<Vehicle*> vehicles;
    
    for (int i = 0; i < numVehicles; i++) {
        Vehicle* v = Initializer::create_vehicle(i);
        v->start();
        vehicles.push_back(v);
    }
    
    std::cout << "All vehicles started. Press Ctrl+C to terminate." << std::endl;
    
    // Set up timeout
    auto start_time = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(timeout_seconds);
    
    // Wait for completion or shutdown
    while (!shutdown_requested) {
        // Check timeout
        auto current_time = std::chrono::steady_clock::now();
        if (current_time - start_time > timeout) {
            std::cout << "Timeout reached after " << timeout_seconds << " seconds. Terminating all vehicles..." << std::endl;
            break;
        }
        
        // Check if any vehicle has exited
        for (auto v = vehicles.begin(); v != vehicles.end();) {
            if (!(*v)->running()) {
                v = vehicles.erase(v);
            } else {
                ++v;
            }
        }
        
        if (vehicles.empty()) {
            std::cout << "All vehicles have completed." << std::endl;
            exit(EXIT_SUCCESS);  // Exit the process when all vehicles are done
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Clean shutdown
    if (!vehicles.empty()) {
        std::cout << "Terminating remaining vehicles..." << std::endl;
        for (auto& v : vehicles) {
            v->stop();
            delete v;
        }
    }
    
    return EXIT_SUCCESS;
}