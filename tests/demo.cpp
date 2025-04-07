#include <iostream>
#include <pthread.h>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <random>
#include <chrono>
#include <sys/stat.h>
#include <errno.h>

#include "initializer.h"
#include "vehicle.h"
#include "debug.h"
#include "component.h"
#include "sender_component.h"
#include "receiver_component.h"

void run_vehicle(Vehicle* v) {
    db<Vehicle>(TRC) << "run_vehicle() called!\n";

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist_lifetime(60, 180);

    // Generating random vehicle lifetime
    int lifetime = dist_lifetime(gen);

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
    db<Vehicle>(INF) << "[Vehicle " << v->id() << "] running for " << lifetime << "s\n";

    // Wait for vehicle lifetime to end
    sleep(lifetime);

    v->stop();
    db<Vehicle>(INF) << "[Vehicle " << v->id() << "] terminated.\n";
}

int main(int argc, char* argv[]) {
    std::cout << "Application started!" << std::endl;

    if (argc < 3) {
        std::cerr << "[ERROR] undefined number of vehicles" << std::endl;
        std::cerr << "Usage: " << argv[0] << " -v number_of_vehicles" << std::endl;
        std::cerr << "Application terminated." << std::endl;
        return -1;
    }

    unsigned int n_vehicles = 0;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-v") {
            if (i + 1 < argc) {
                try {
                    n_vehicles = std::stoi(argv[++i]);
                    break;
                } catch (const std::exception& e) {
                    std::cerr << "[ERROR] invalid number of vehicles" << std::endl;
                    std::cerr << "Must be an integer between 1 and 10" << std::endl;
                    std::cerr << "Application terminated." << std::endl;
                    return -1;
                }
            }
        }
    }

    if (n_vehicles <= 0 || n_vehicles > 10) {
        std::cerr << "[ERROR] invalid number of vehicles" << std::endl;
        std::cerr << "Must be an integer between 1 and 10" << std::endl;
        std::cerr << "Application terminated." << std::endl;
        return -1;
    }

    // Create logs directory if it doesn't exist
    mkdir("./logs", 0777);

    std::vector<pid_t> children;

    for (unsigned int id = 1; id <= n_vehicles; ++id) {
        pid_t pid = fork();

        if (pid < 0) {
            std::cerr << "[ERROR] failed to fork process" << std::endl;
            std::cerr << "Application terminated." << std::endl;
            return -1;
        }

        if (pid == 0) {
            std::string log_file = "./logs/vehicle_" + std::to_string(id) + ".log";
            Debug::set_log_file(log_file);

            std::cout << "[Child " << getpid() << "] creating vehicle " << id << std::endl;
            Vehicle* v = Initializer::create_vehicle(id);
            run_vehicle(v);
            
            delete v;
            Debug::close_log_file();
            std::cout << "[Child " << getpid() << "] vehicle " << id << " finished execution" << std::endl;

            exit(0);
        } else {
            children.push_back(pid);
        }
    }

    for (pid_t child_pid : children) {
        int status;
        if (waitpid(child_pid, &status, 0) == -1) {
            std::cerr << "[ERROR] failed to wait for child " << child_pid << std::endl;
            std::cerr << "Application terminated." << std::endl;
            return -1;
        } else {
            std::cout << "[Parent] child " << child_pid << " terminated with status " << status << std::endl;
        }
    }

    std::cout << "Application completed successfully!" << std::endl;
    return 0;
}