#ifndef RECEIVER_COMPONENT_H
#define RECEIVER_COMPONENT_H

#include "component.h"
#include "vehicle.h"
#include "debug.h"
#include <chrono>
#include <regex>
#include <string>
#include <csignal>
#include <cerrno>

class ReceiverComponent : public Component {
public:
    ReceiverComponent(Vehicle* vehicle)
        : Component(vehicle, "Receiver") {
        std::string log_file = "./logs/vehicle_" + std::to_string(vehicle->id()) + "_receive.csv";
        open_log_file(log_file);
        
        // Override the default header with one that includes latency information
        if (_log_file.is_open()) {
            _log_file.seekp(0);
            _log_file << "receive_timestamp,source_vehicle,message_id,event_type,send_timestamp,latency_us\n";
            _log_file.flush();
        }
    }

    void start() override {
        _running = true;
        _thread = std::thread(&ReceiverComponent::run, this);
    }
    
    void stop() override {
        db<Vehicle>(INF) << "[Vehicle " << _vehicle->id() << "] stopping receiver thread.\n";
        Component::stop();
    }

private:
    void run() {
        db<Vehicle>(TRC) << "ReceiverComponent::run() started for vehicle " << _vehicle->id() << "\n";

        while (_running) {
            // Check running status before blocking on receive
            if (!_running) {
                db<Vehicle>(TRC) << "[Vehicle " << _vehicle->id() << "] receiver thread check - detected stop flag before receive\n";
                break;
            }
            
            unsigned int size = Vehicle::MAX_MESSAGE_SIZE;
            char buf[size];

            int result = _vehicle->receive(buf, size);

            // First check if we're still running after the receive call
            if (!_running) {
                db<Vehicle>(TRC) << "[Vehicle " << _vehicle->id() << "] receive loop interrupted by stop flag check after receive().\n";
                break;
            }

            if (result < 0) {
                // Error occurred
                if (errno == EINTR) {
                    // Interrupted by signal (SIGUSR1)!
                    db<Vehicle>(TRC) << "[Vehicle " << _vehicle->id() << "] receive() interrupted by signal (EINTR). Checking running flag.\n";
                    continue;
                } else {
                    // A real receive error
                    if (_running) {
                        db<Vehicle>(ERR) << "[Vehicle " << _vehicle->id() << "] failed to receive message: " << strerror(errno) << " (errno=" << errno << ")\n";
                    } else {
                        // This is expected when component is stopped and connections are closed
                        db<Vehicle>(TRC) << "[Vehicle " << _vehicle->id() << "] receive call returned error after stop (expected).\n";
                        break;
                    }
                }
            } else if (result == 0) {
                // Connection closed by peer (or potentially by v->stop() if it closes the FD)
                if (_running) {
                    db<Vehicle>(INF) << "[Vehicle " << _vehicle->id() << "] receive returned 0 (connection closed?).\n";
                } else {
                    // This is expected when component is stopped and connections are closed
                    db<Vehicle>(TRC) << "[Vehicle " << _vehicle->id() << "] receive returned 0 after stop (expected).\n";
                    break;
                }
            } else if (result > 0) {
                auto recv_time = std::chrono::steady_clock::now();
                auto recv_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
                    recv_time.time_since_epoch()).count();
                
                std::string received_message(buf, result);
                db<Vehicle>(INF) << "[Vehicle " << _vehicle->id() << "] message received: " << received_message << "\n";
                
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
                    if (_log_file.is_open()) {
                        _log_file << recv_time_us << "," << source_vehicle << "," << message_id 
                                << ",receive," << send_time_us << "," << latency_us << "\n";
                        _log_file.flush();
                    }
                    
                    // Also log human-readable latency
                    db<Vehicle>(INF) << "[Vehicle " << _vehicle->id() 
                              << "] received message from Vehicle " << source_vehicle 
                              << ", msg_id=" << message_id
                              << ", latency=" << latency_us << "μs ("
                              << (latency_us / 1000.0) << "ms)\n";
                } else {
                    // Log with unknown values if pattern matching fails
                    if (_log_file.is_open()) {
                        _log_file << recv_time_us << ",unknown,unknown,receive,unknown,unknown\n";
                        _log_file.flush();
                    }
                }
            }
            
            // Check if vehicle is still running - helps break the loop sooner
            if (!_vehicle->running()) {
                db<Vehicle>(TRC) << "[Vehicle " << _vehicle->id() << "] detected vehicle is no longer running, exiting receiver loop\n";
                break;
            }
        }

        db<Vehicle>(INF) << "[Vehicle " << _vehicle->id() << "] receiver component terminated.\n";
    }
};

#endif // RECEIVER_COMPONENT_H 