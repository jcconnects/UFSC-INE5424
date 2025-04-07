#ifndef RECEIVER_COMPONENT_H
#define RECEIVER_COMPONENT_H

#include "component.h"
#include "debug.h"
#include <chrono>
#include <regex>
#include <string>

class ReceiverComponent : public Component {
public:
    ReceiverComponent(Vehicle* vehicle)
        : Component(vehicle, "Receiver") {
        std::string log_file = "./logs/vehicle_" + std::to_string(vehicle->id()) + "_receive.csv";
        open_log_file(log_file);
        
        // Override the default header with one that includes latency information
        if (_log_file.is_open()) {
            _log_file.seekp(0);
            _log_file << "receive_timestamp,source_vehicle,message_id,event_type,send_timestamp,latency_ms\n";
            _log_file.flush();
        }
    }

    void start() override {
        _running = true;
        _thread = std::thread(&ReceiverComponent::run, this);
    }

private:
    void run() {
        db<Vehicle>(TRC) << "ReceiverComponent::run() started for vehicle " << _vehicle->id() << "\n";

        while (_running) {
            unsigned int size = Vehicle::MAX_MESSAGE_SIZE;
            char buf[size];

            int result = _vehicle->receive(buf, size);

            if (result > 0) {
                auto recv_time = std::chrono::steady_clock::now();
                auto recv_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    recv_time.time_since_epoch()).count();
                
                std::string received_message(buf, result);
                db<Vehicle>(INF) << "[Vehicle " << _vehicle->id() << "] message received: " << received_message << "\n";
                
                // Extract information from the message using regex
                std::regex pattern("Vehicle (\\d+) message (\\d+) at (\\d+)");
                std::smatch matches;
                
                if (std::regex_search(received_message, matches, pattern) && matches.size() > 3) {
                    int source_vehicle = std::stoi(matches[1]);
                    int message_id = std::stoi(matches[2]);
                    long long send_time = std::stoll(matches[3]);
                    
                    // Calculate latency
                    long long latency = recv_time_ms - send_time;
                    
                    // Log to CSV with latency information
                    if (_log_file.is_open()) {
                        _log_file << recv_time_ms << "," << source_vehicle << "," << message_id 
                                 << ",receive," << send_time << "," << latency << "\n";
                        _log_file.flush();
                    }
                } else {
                    // Log with unknown values if pattern matching fails
                    if (_log_file.is_open()) {
                        _log_file << recv_time_ms << ",unknown,unknown,receive,unknown,unknown\n";
                        _log_file.flush();
                    }
                }
            } else {
                db<Vehicle>(INF) << "[Vehicle " << _vehicle->id() << "] failed to receive message\n";
            }
        }

        db<Vehicle>(INF) << "[Vehicle " << _vehicle->id() << "] receiver component terminated.\n";
    }
};

#endif // RECEIVER_COMPONENT_H 