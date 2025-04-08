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
    SenderComponent(Vehicle* vehicle)
        : Component(vehicle, "Sender") {
        std::string log_file = "./logs/vehicle_" + std::to_string(vehicle->id()) + "_send.csv";
        open_log_file(log_file);
        
        // Override the default header
        if (_log_file.is_open()) {
            _log_file.seekp(0);
            _log_file << "timestamp,source_vehicle,message_id,event_type\n";
            _log_file.flush();
        }
    }

    void start() override {
        _running = true;
        _thread = std::thread(&SenderComponent::run, this);
    }
    
    void stop() override {
        db<Vehicle>(INF) << "[Vehicle " << _vehicle->id() << "] sender component stopping.\n";
        Component::stop();
    }

private:
    void run() {
        db<Vehicle>(TRC) << "SenderComponent::run() started for vehicle " << _vehicle->id() << "\n";

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> delay_dist(5, 10);

        int counter = 1;

        while (_running) {
            auto now = std::chrono::steady_clock::now();
            auto time_us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
            
            std::string msg = "Vehicle " + std::to_string(_vehicle->id()) + " message " + std::to_string(counter) + " at " + std::to_string(time_us);
            
            db<Vehicle>(INF) << "[Vehicle " << _vehicle->id() << "] sending message " << counter << ": {" << msg << "}\n";
            
            if (_vehicle->send(msg.c_str(), msg.size())) {
                db<Vehicle>(INF) << "[Vehicle " << _vehicle->id() << "] message " << counter << " sent!\n";
                
                // Log to CSV
                if (_log_file.is_open()) {
                    _log_file << time_us << "," << _vehicle->id() << "," << counter << ",send\n";
                    _log_file.flush();
                }
            } else {
                db<Vehicle>(INF) << "[Vehicle " << _vehicle->id() << "] failed to send message " << counter << "!\n";
            }
            
            counter++;

            // Random wait between messages
            int wait_time = delay_dist(gen);
            sleep(wait_time);
        }

        db<Vehicle>(INF) << "[Vehicle " << _vehicle->id() << "] sender component terminated.\n";
    }
};

#endif // SENDER_COMPONENT_H 