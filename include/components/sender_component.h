#ifndef SENDER_COMPONENT_H
#define SENDER_COMPONENT_H

#include "component.h"
#include "vehicle.h"
#include "debug.h"
#include <chrono>
#include <random>
#include <unistd.h>

class SenderComponent : public Component {
public:
    SenderComponent(Vehicle* vehicle);

    void start() override;
    
private:
    static void* run(void* arg);
};

/*************** Sender Component Implementation ******************/
SenderComponent::SenderComponent(Vehicle* vehicle) : Component(vehicle, "Sender") {
    std::string log_file = "./logs/vehicle_" + std::to_string(vehicle->id()) + "_sender.csv";
    open_log_file(log_file);
        
    // Override the default header
    if (_log_file.is_open()) {
        _log_file.seekp(0);
        _log_file << "timestamp,source_vehicle,message_id,event_type\n";
        _log_file.flush();
    }
}

void SenderComponent::start() {
    db<Component>(TRC) << "SenderComponent::start() called\n";

    _running = true;
    pthread_create(&_thread, nullptr, SenderComponent::run, this);
}

void* SenderComponent::run(void* arg) {
    db<Component>(TRC) << "SenderComponent::run() called!\n";

    SenderComponent* c = static_cast<SenderComponent*>(arg);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> delay_dist(20, 40);

    int counter = 1;

    while (c->vehicle()->running()) {
        auto now = std::chrono::steady_clock::now();
        auto time_us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
        
        std::string msg = "Vehicle " + std::to_string(c->vehicle()->id()) + " message " + std::to_string(counter) + " at " + std::to_string(time_us);
        
        db<Component>(INF) << "[SenderComponent " << c->vehicle()->id() << "] sending message " << counter << ": {" << msg << "}\n";
        
        if (c->vehicle()->send(msg.c_str(), msg.size())) {
            db<Component>(INF) << "[SenderComponent " << c->vehicle()->id() << "] message " << counter << " sent!\n";
            
            // Log to CSV
            if (c->log_file()->is_open()) {
                (*c->log_file()) << time_us << "," << c->vehicle()->id() << "," << counter << ",send\n";
                c->log_file()->flush();
            }
        } else {
            db<Component>(INF) << "[SenderComponent " << c->vehicle()->id() << "] failed to send message " << counter << "!\n";
        }
        
        counter++;

        // Random wait between messages
        int wait_time = delay_dist(gen);
        sleep(wait_time);
    }

    db<Component>(INF) << "[SenderComponent " << c->vehicle()->id() << "] terminated.\n";
    return nullptr;
}

#endif // SENDER_COMPONENT_H 