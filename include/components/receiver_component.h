#ifndef RECEIVER_COMPONENT_H
#define RECEIVER_COMPONENT_H

#include <chrono>
#include <regex>
#include <string>
#include <csignal>
#include <cerrno>
#include <sstream>
#include <pthread.h>
#include <time.h>

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
    db<Component>(TRC) << "ReceiverComponent::stop() called for vehicle " << vehicle()->id() << "\n";
    
    // Set running to false to signal the thread to exit
    _running = false;
    
    // Try to join with a timeout
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 3; // 3 second timeout
    
    db<Component>(TRC) << "[ReceiverComponent " << vehicle()->id() << "] attempting to join thread\n";
    
    int join_result = pthread_timedjoin_np(_thread, nullptr, &ts);
    if (join_result == 0) {
        db<Component>(TRC) << "[ReceiverComponent " << vehicle()->id() << "] thread joined successfully\n";
    } else if (join_result == ETIMEDOUT) {
        db<Component>(ERR) << "[ReceiverComponent " << vehicle()->id() << "] thread join timed out, may have deadlocked\n";
    } else {
        db<Component>(ERR) << "[ReceiverComponent " << vehicle()->id() << "] thread join failed with error: " << join_result << "\n";
    }
    
    db<Component>(INF) << "[ReceiverComponent " << vehicle()->id() << "] terminated.\n";
}

void* ReceiverComponent::run(void* arg)  {
    db<Component>(TRC) << "ReceiverComponent::run() called!\n";

    ReceiverComponent* c = static_cast<ReceiverComponent*>(arg);

    while (c->running() && c->vehicle()->running()) {
        unsigned int size = Vehicle::MAX_MESSAGE_SIZE;
        char buf[size];

        // Check running status right before receive call
        if (!c->running() || !c->vehicle()->running()) {
            break;
        }

        // Set a timeout for receive calls so we can check running state more frequently
        int result = c->vehicle()->receive(buf, size);

        // Check if we're still supposed to be running after receive returns
        if (!c->running() || !c->vehicle()->running()) {
            db<Component>(TRC) << "[ReceiverComponent " << c->vehicle()->id() << "] exiting due to stop signal\n";
            break;
        }

        // Only process result if we got data
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
                db<Component>(INF) << "[ReceiverComponent " << c->vehicle()->id() 
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
        }
    }

    db<Component>(TRC) << "[ReceiverComponent " << c->vehicle()->id() << "] run loop exited\n";
    return nullptr;
}

#endif // RECEIVER_COMPONENT_H 