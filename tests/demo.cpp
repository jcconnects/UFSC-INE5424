#include <iostream>
#include <pthread.h>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <random>
#include <chrono>

#include "initializer.h"
#include "vehicle.h"
#include "debug.h"

void send_run(Vehicle* v) {
    db<Vehicle>(TRC) << "send_run() called!\n";

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> delay_dist(5, 10); // intervalo aleatório entre envios

    int counter = 1;

    while (v->running()) {
        auto now = std::chrono::steady_clock::now();

        // Mensagem de exemplo
        auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        std::string msg = "Vehicle " + std::to_string(v->id()) + " message " + std::to_string(counter) + " at " + std::to_string(time_ms);
        
        db<Vehicle>(INF) << "[Vehicle " << v->id() << "] sending message " << counter << ": {" << msg << "}\n";
        if (v->send(msg.c_str(), msg.size())) {
           db<Vehicle>(INF) << "[Vehicle " << v->id() << "] message " << counter << " sent!\n";
        } else {
           db<Vehicle>(INF) << "[Vehicle " << v->id() << "] failed to send message " << counter << "!\n";
        }
        
        counter++;

        // Espera aleatória entre 1 e 3 segundos
        int wait_time = delay_dist(gen);
        sleep(wait_time);
    }

   db<Vehicle>(INF) << "[Vehicle " << v->id() << "] send_thread terminated.\n";
}

void receive_run(Vehicle* v) {
    db<Vehicle>(TRC) << "receive_run() called!\n";

    while (v->running()) {
        unsigned int size = Vehicle::MAX_MESSAGE_SIZE;
        char buf[size];

        int result = v->receive(buf, size);

        if (result == 0) {
            db<Vehicle>(INF) << "[Vehicle " << v->id() << "] failed to receive message\n";
        } else {
            std::string received_message(buf, result);
            db<Vehicle>(INF) << "[Vehicle " << v->id() << "] message received: " << received_message << "\n";
        }
    }

   db<Vehicle>(INF) << "[Vehicle " << v->id() << "] receive_thread terminated.\n";
}

void* send_thread_entry(void* arg) {
    Vehicle* v = static_cast<Vehicle*>(arg);
    send_run(v);
    return nullptr;
}

void* receive_thread_entry(void* arg) {
    Vehicle* v = static_cast<Vehicle*>(arg);
    receive_run(v);
    return nullptr;
}

void run_vehicle(Vehicle* v) {
    db<Vehicle>(TRC) << "run_vehicle() called!\n";

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist_lifetime(60, 180);

    // Generating random vehicle lifetime
    int lifetime = dist_lifetime(gen);

    pthread_t send_thread;
    pthread_t receive_thread;

    v->start();
    db<Vehicle>(INF) << "[Vehicle " << v->id() << "] running for " << lifetime << "s\n";

    // Creating threads
    if (v->id() == 1) {
        pthread_create(&send_thread, nullptr, send_thread_entry, v);
    }
    pthread_create(&receive_thread, nullptr, receive_thread_entry, v);

    // Waits for vehicle lifetime to end;
    sleep(lifetime);

    pthread_join(receive_thread, nullptr);

    if (v->id() == 1){   
        pthread_join(send_thread, nullptr);
    }

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
                } catch (const std::exception e) {
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

            Debug::close_log_file();
            std::cout << "[Child " << getpid() <<"] vehicle " << id << "finished execution" << std::endl;

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