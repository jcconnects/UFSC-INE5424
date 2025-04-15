#ifndef RECEIVER_COMPONENT_H
#define RECEIVER_COMPONENT_H

#include <chrono>
#include <regex>
#include <string>
#include <csignal>
#include <cerrno>
#include <sstream>
#include <time.h>
#include <unistd.h>

#include "component.h"
#include "vehicle.h"
#include "debug.h"


class ReceiverComponent : public Component {
    public:
        ReceiverComponent(Vehicle* vehicle);

        void start() override;
        void signal_stop() override;
        void stop() override;
        
    private:
        static void* run(void* arg);
};

/***************** Receiver Component Implementation ********************/

ReceiverComponent::ReceiverComponent(Vehicle* vehicle) : Component(vehicle, "Receiver") {
    std::string log_file = "./logs/vehicle_" + std::to_string(vehicle->id()) + "_receiver.csv";
    open_log_file(log_file);
    
    // Override the default header with one that includes latency information
    write_to_log("receive_timestamp,source_vehicle,message_id,event_type,send_timestamp,latency_us\n");
}

void ReceiverComponent::start() {
    db<Component>(TRC) << "ReceiverComponent::start() called\n";

    _running = true;
    pthread_create(&_thread, nullptr, ReceiverComponent::run, this);
}

void ReceiverComponent::signal_stop() {
    db<Component>(TRC) << "ReceiverComponent::signal_stop() called\n";
    Component::signal_stop();
}

void ReceiverComponent::stop() {
    db<Component>(TRC) << "ReceiverComponent::stop() called\n";
    Component::stop();
}

void* ReceiverComponent::run(void* arg)  {
    db<Component>(TRC) << "ReceiverComponent::run() called!\n";

    ReceiverComponent* c = static_cast<ReceiverComponent*>(arg);
    Vehicle* vehicle = c->vehicle(); // Get vehicle pointer once
    unsigned int vehicle_id = vehicle->id(); // Get ID once

    // Simplified receive loop with reliable shutdown checks
    while (vehicle->running() && c->running()) {
        unsigned int size = Vehicle::MAX_MESSAGE_SIZE;
        char buf[size];

        // Critical section: Receive message (can block)
        int result = vehicle->receive(buf, size);
        
        // Immediately check running status after receive returns
        if (!vehicle->running() || !c->running()) {
            db<Component>(TRC) << "[ReceiverComponent " << vehicle_id << "] Detected stop after receive() returned.\n";
            break;
        }

        // Process the received message if any
        if (result > 0) {
            auto recv_time = std::chrono::steady_clock::now();
            auto recv_time_us = std::chrono::duration_cast<std::chrono::microseconds>(recv_time.time_since_epoch()).count();
            
            std::string received_message(buf, result);
            
            // Extract information from the message using regex
            std::regex pattern("Vehicle (\\d+) message (\\d+) at (\\d+)");
            std::smatch matches;
            
            if (std::regex_search(received_message, matches, pattern) && matches.size() > 3) {
                int source_vehicle = std::stoi(matches[1]);
                int message_id = std::stoi(matches[2]);
                long long send_time_us = std::stoll(matches[3]);
                
                // Calculate latency in microseconds
                long long latency_us = recv_time_us - send_time_us;
                
                // Thread-safe log to CSV with latency information
                std::stringstream log_line;
                log_line << recv_time_us << "," << source_vehicle << "," << message_id 
                        << ",receive," << send_time_us << "," << latency_us << "\n";
                c->write_to_log(log_line.str());
                
                // Also log human-readable latency
                db<Component>(INF) << "[ReceiverComponent " << vehicle_id 
                        << "] received message from Vehicle " << source_vehicle 
                        << ", msg_id = " << message_id
                        << ", latency = " << latency_us << "μs ("
                        << (latency_us / 1000.0) << "ms)\n";
            } else {
                // Thread-safe log with unknown values if pattern matching fails
                std::stringstream log_line;
                log_line << recv_time_us << ",unknown,unknown,receive,unknown,unknown\n";
                c->write_to_log(log_line.str());
            }
            
            db<Component>(TRC) << "[ReceiverComponent " << vehicle_id << "] Successfully processed received message (" << result << " bytes).\n";
        } else if (result < 0) {
            // Error case
            db<Component>(ERR) << "[ReceiverComponent " << vehicle_id << "] receive() returned error code: " << result << "\n";
        } else if (result == 0) {
            // No message case (likely due to shutdown or timeout)
            db<Component>(TRC) << "[ReceiverComponent " << vehicle_id << "] receive() returned 0 (no message).\n";
            
            // Short sleep to avoid spinning
            usleep(5000); // 5ms sleep
            
            // Check running status again after sleep
            if (!vehicle->running() || !c->running()) {
                db<Component>(TRC) << "[ReceiverComponent " << vehicle_id << "] Detected stop after sleep.\n";
                break;
            }
        }
    }

    db<Component>(INF) << "[ReceiverComponent " << vehicle_id << "] Run loop exited. Terminating thread.\n";
    return nullptr;
}

#endif // RECEIVER_COMPONENT_H 