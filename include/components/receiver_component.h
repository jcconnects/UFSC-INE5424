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

void ReceiverComponent::stop() {
    Component::stop();
}

void* ReceiverComponent::run(void* arg)  {
    db<Component>(TRC) << "ReceiverComponent::run() called!\n";

    ReceiverComponent* c = static_cast<ReceiverComponent*>(arg);
    Vehicle* vehicle = c->vehicle(); // Get vehicle pointer once
    unsigned int vehicle_id = vehicle->id(); // Get ID once

    // Use a shorter processing cycle to check vehicle status more frequently
    while (true) {
        // Check running status before blocking receive call
        if (!vehicle->running()) {
            db<Component>(TRC) << "[ReceiverComponent " << vehicle_id << "] Detected vehicle stopped before receive(). Exiting loop.\n";
            break;
        }

        unsigned int size = Vehicle::MAX_MESSAGE_SIZE;
        char buf[size];

        // Add timeout check for running status - check every 50ms
        struct timespec start, now;
        clock_gettime(CLOCK_MONOTONIC, &start);
        bool should_exit = false;

        // Set up a timeout loop to periodically check running status
        while (vehicle->running()) {
            // Try a non-blocking receive or one with a short timeout
            int result = vehicle->receive(buf, size);
            
            // If receive returned due to a message or error, process it
            if (result != 0) {
                // Check running status immediately after receive returns
                if (!vehicle->running()) {
                    db<Component>(TRC) << "[ReceiverComponent " << vehicle_id << "] Detected vehicle stopped after receive() returned.\n";
                    should_exit = true;
                    break; // Exit inner loop
                }

                // Process the result if the vehicle is supposed to be running
                if (result < 0) {
                    // Negative result indicates an error (e.g., buffer too small)
                    db<Component>(ERR) << "[ReceiverComponent " << vehicle_id << "] receive() returned error code: " << result << "\n";
                    should_exit = true;
                    break; // Break on errors
                } else { // result > 0
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
                }
                
                // Break inner loop after processing a message
                break;
            }
            
            // Check if we've been waiting too long without receiving anything
            clock_gettime(CLOCK_MONOTONIC, &now);
            double elapsed_ms = (now.tv_sec - start.tv_sec) * 1000.0 + (now.tv_nsec - start.tv_nsec) / 1000000.0;
            
            // Check running status every 50ms if no message is received
            if (elapsed_ms > 50.0) {
                db<Component>(TRC) << "[ReceiverComponent " << vehicle_id << "] No message received for 50ms, checking running status.\n";
                
                // Update start time for next iteration
                start = now;
                if (!vehicle->running()) {
                    should_exit = true;
                    break;
                }
                
                // No need to break - let the loop condition check vehicle->running()
            } else {
                // Short sleep to avoid tight spinning
                usleep(5000); // 5ms
            }
        }
        
        if (should_exit || !vehicle->running()) {
            break; // Exit outer loop
        }
    }

    db<Component>(INF) << "[ReceiverComponent " << vehicle_id << "] Run loop finished. Terminating thread.\n";
    return nullptr;
}

#endif // RECEIVER_COMPONENT_H 