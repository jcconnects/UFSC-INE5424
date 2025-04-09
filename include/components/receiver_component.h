#ifndef RECEIVER_COMPONENT_H
#define RECEIVER_COMPONENT_H

#include <chrono>
#include <regex>
#include <string>
#include <csignal>
#include <cerrno>

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
    if (_log_file.is_open()) {
        _log_file.seekp(0);
        _log_file << "receive_timestamp,source_vehicle,message_id,event_type,send_timestamp,latency_us\n";
        _log_file.flush();
    }
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

    while (c->vehicle()->running()) {
        unsigned int size = Vehicle::MAX_MESSAGE_SIZE;
        char buf[size];

        int result = c->vehicle()->receive(buf, size);

        if (result <= 0) {
            db<Component>(INF) << "[ReceiverComponent " << c->vehicle()->id() << "] failed to receive message\n";
        } else if (result > 0) {
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
                
                // Log to CSV with latency information
                if (c->log_file()->is_open()) {
                    (*c->log_file()) << recv_time_us << "," << source_vehicle << "," << message_id << ",receive," << send_time_us << "," << latency_us << "\n";
                    c->log_file()->flush();
                }
                
                // Also log human-readable latency
                db<Component>(INF) << "[ReceiverComponent " << c->vehicle()->id() 
                        << "] received message from Vehicle " << source_vehicle 
                        << ", msg_id = " << message_id
                        << ", latency = " << latency_us << "μs ("
                        << (latency_us / 1000.0) << "ms)\n";
            } else {
                // Log with unknown values if pattern matching fails
                if (c->log_file()->is_open()) {
                    (*c->log_file()) << recv_time_us << ",unknown,unknown,receive,unknown,unknown\n";
                    c->log_file()->flush();
                }
            }
        }
    }

    db<Component>(INF) << "[ReceiverComponent " << c->vehicle()->id() << "] terminated.\n";
    return nullptr;
}

#endif // RECEIVER_COMPONENT_H 