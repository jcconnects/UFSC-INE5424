#ifndef SENDER_COMPONENT_H
#define SENDER_COMPONENT_H

#include "component.h"
#include "vehicle.h"
#include "debug.h"
#include <chrono>
#include <random>
#include <unistd.h>
#include <sstream>
#include <time.h>

class SenderComponent : public Component {
public:
    SenderComponent(Vehicle* vehicle);

    void start() override;
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

void SenderComponent::stop() {
    db<Component>(TRC) << "SenderComponent::stop() called for vehicle " << vehicle()->id() << "\n";
    
    // Set running to false to signal the thread to exit
    _running = false;
    
    // Try to join with a timeout
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 3; // 3 second timeout
    
    db<Component>(TRC) << "[SenderComponent " << vehicle()->id() << "] attempting to join thread\n";
    
    int join_result = pthread_timedjoin_np(_thread, nullptr, &ts);
    if (join_result == 0) {
        db<Component>(TRC) << "[SenderComponent " << vehicle()->id() << "] thread joined successfully\n";
    } else if (join_result == ETIMEDOUT) {
        db<Component>(ERR) << "[SenderComponent " << vehicle()->id() << "] thread join timed out, may have deadlocked\n";
    } else {
        db<Component>(ERR) << "[SenderComponent " << vehicle()->id() << "] thread join failed with error: " << join_result << "\n";
    }
    
    db<Component>(INF) << "[SenderComponent " << vehicle()->id() << "] terminated.\n";
}

void* SenderComponent::run(void* arg) {
    db<Component>(TRC) << "SenderComponent::run() called!\n";

    SenderComponent* c = static_cast<SenderComponent*>(arg);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> delay_dist(100, 1000);

    int counter = 1;

    while (c->running() && c->vehicle()->running()) {
        // Check running status at start of iteration
        if (!c->running() || !c->vehicle()->running()) {
            break;
        }
        
        auto now = std::chrono::steady_clock::now();
        auto time_us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
        
        std::string msg = "Vehicle " + std::to_string(c->vehicle()->id()) + " message " + std::to_string(counter) + " at " + std::to_string(time_us);
        
        db<Component>(INF) << "[SenderComponent " << c->vehicle()->id() << "] sending message " << counter << ": {" << msg << "}\n";
        
        // Check running again before sending
        if (!c->running() || !c->vehicle()->running()) {
            db<Component>(TRC) << "[SenderComponent " << c->vehicle()->id() << "] exiting due to stop signal before send\n";
            break;
        }
        
        if (c->vehicle()->send(msg.c_str(), msg.size())) {
            db<Component>(INF) << "[SenderComponent " << c->vehicle()->id() << "] message " << counter << " sent!\n";
            
            // Thread-safe log to CSV using string stream
            std::stringstream log_line;
            log_line << time_us << "," << c->vehicle()->id() << "," << counter << ",send\n";
            c->write_to_log(log_line.str());
        } else {
            db<Component>(INF) << "[SenderComponent " << c->vehicle()->id() << "] failed to send message " << counter << "!\n";
        }
        
        counter++;

        // Random wait between messages - using microseconds (1/1000 of a second)
        int wait_time_ms = delay_dist(gen);
        
        // Check running status before sleeping
        if (!c->running() || !c->vehicle()->running()) {
            db<Component>(TRC) << "[SenderComponent " << c->vehicle()->id() << "] exiting due to stop signal before sleep\n";
            break;
        }
        
        usleep(wait_time_ms * 1000); // Convert milliseconds to microseconds
        
        // Check running status again after sleep
        if (!c->running() || !c->vehicle()->running()) {
            db<Component>(TRC) << "[SenderComponent " << c->vehicle()->id() << "] exiting due to stop signal after sleep\n";
            break;
        }
    }

    db<Component>(TRC) << "[SenderComponent " << c->vehicle()->id() << "] run loop exited\n";
    return nullptr;
}

#endif // SENDER_COMPONENT_H 