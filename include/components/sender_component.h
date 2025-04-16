#ifndef SENDER_COMPONENT_H
#define SENDER_COMPONENT_H

#include "component.h"
#include "vehicle.h"
#include "debug.h"
#include <chrono>
#include <random>
#include <unistd.h>
#include <sstream>

class SenderComponent : public Component {
public:
    SenderComponent(Vehicle* vehicle);

    void start() override;
    void signal_stop() override;
    void stop() override;
    
private:
    static void* run(void* arg);
};

/*************** Sender Component Implementation ******************/
SenderComponent::SenderComponent(Vehicle* vehicle) : Component(vehicle, "Sender") {
    std::string log_file = "./logs/vehicle_" + std::to_string(vehicle->id()) + "_sender.csv";
    open_log_file(log_file);
        
    // Override the default header
    write_to_log("timestamp,source_vehicle,message_id,event_type\n");
}

void SenderComponent::start() {
    db<Component>(TRC) << "SenderComponent::start() called\n";

    _running = true;
    pthread_create(&_thread, nullptr, SenderComponent::run, this);
}

void SenderComponent::signal_stop() {
    db<Component>(TRC) << "SenderComponent::signal_stop() called\n";
    Component::signal_stop();
}

void SenderComponent::stop() {
    db<Component>(TRC) << "SenderComponent::stop() called\n";
    Component::stop();
}

void* SenderComponent::run(void* arg) {
    db<Component>(TRC) << "SenderComponent::run() called!\n";

    SenderComponent* c = static_cast<SenderComponent*>(arg);
    Vehicle* vehicle = c->vehicle(); // Get vehicle pointer once
    unsigned int vehicle_id = vehicle->id(); // Get ID once

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> delay_dist(0, 50);

    int counter = 1;

    // Check both vehicle running status and component running status
    while (vehicle->running() && c->running()) {
        auto now = std::chrono::steady_clock::now();
        auto time_us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
        
        std::string msg = "Vehicle " + std::to_string(vehicle_id) + " message " + std::to_string(counter) + " at " + std::to_string(time_us);
        
        db<Component>(INF) << "[SenderComponent " << vehicle_id << "] sending message " << counter << ": {" << msg << "}\n";
        
        // Critical section: send message and immediately check running status
        int send_result = vehicle->send(msg.c_str(), msg.size());
        
        // Immediately check running status after vehicle->send() returns
        if (!vehicle->running() || !c->running()) {
            db<Component>(TRC) << "[SenderComponent " << vehicle_id << "] Detected stop condition after send().\n";
            break;
        }
        
        if (send_result) {
            db<Component>(INF) << "[SenderComponent " << vehicle_id << "] message " << counter << " sent!\n";
            
            // Thread-safe log to CSV using string stream
            std::stringstream log_line;
            log_line << time_us << "," << vehicle_id << "," << counter << ",send\n";
            c->write_to_log(log_line.str());
        } else {
            db<Component>(INF) << "[SenderComponent " << vehicle_id << "] failed to send message " << counter << "!\n";
            
            // Check if failure was due to shutdown
            if (!vehicle->running() || !c->running()) {
                db<Component>(TRC) << "[SenderComponent " << vehicle_id << "] Send failed due to shutdown.\n";
                break;
            }
        }
        
        counter++;

        // Random wait between messages - break it into smaller sleeps with status checks
        int wait_time_ms = delay_dist(gen);
        
        // Break the wait into smaller segments to check running status more frequently
        int remaining_ms = wait_time_ms;
        while (remaining_ms > 0) {
            // Sleep in smaller chunks (5ms) to check running status more frequently
            int sleep_this_time = std::min(remaining_ms, 5);
            usleep(sleep_this_time * 1000); // Convert milliseconds to microseconds
            remaining_ms -= sleep_this_time;
            
            // Check running status after each short sleep
            if (!vehicle->running() || !c->running()) {
                db<Component>(TRC) << "[SenderComponent " << vehicle_id << "] Detected stop during sleep.\n";
                break;
            }
        }
        
        // Check running status again after sleep
        if (!vehicle->running() || !c->running()) {
            break;
        }
    }

    db<Component>(INF) << "[SenderComponent " << vehicle_id << "] Run loop exited. Terminating thread.\n";
    return nullptr;
}

#endif // SENDER_COMPONENT_H 