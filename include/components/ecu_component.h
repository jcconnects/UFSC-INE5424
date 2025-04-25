#ifndef ECU_COMPONENT_H
#define ECU_COMPONENT_H

#include <chrono>
#include <regex>
#include <string>
#include <csignal>
#include <cerrno>
#include <array>
#include <algorithm> // for std::replace

#include "component.h"
#include "debug.h"

class ECUComponent : public Component {
public:
    ECUComponent(Vehicle* vehicle, const std::string& name, TheProtocol* protocol, TheAddress address)
        : Component(vehicle, name, protocol, address)
    {
        // Open log file with ECU specific prefix
        open_log_file("ecu_log");

        if (_log_file.is_open()) {
            _log_file.seekp(0); // Go to beginning to overwrite if file exists
            // Define log header
            _log_file << "receive_timestamp_us,source_address,source_component_type,source_vehicle,message_id,event_type,send_timestamp_us,latency_us,raw_message\n";
            _log_file.flush();
        }
    }

    // The main loop for the ECU component thread
    void run() override {
        db<ECUComponent>(INF) << "[ECU " << Component::getName() << "] thread running.\n";

        std::array<char, TheCommunicator::MAX_MESSAGE_SIZE> buf;
        TheAddress source_addr;

        while (running()) {
            int bytes_received = receive(buf.data(), buf.size(), &source_addr);

            if (bytes_received < 0) {
                // Receive was interrupted, likely by stop()
                 db<ECUComponent>(INF) << "[ECU " << Component::getName() << "] receive interrupted.\n";
                break; // Exit the loop
            } else if (bytes_received == 0) {
                // Receive failed or timed out, but we're still running
                // db<ECUComponent>(WRN) << "[ECU " << Component::getName() << "] receive failed or timed out.\n"; // Potentially noisy
                continue; // Try receiving again
            } else {
                // Message received successfully
                auto recv_time_system = std::chrono::system_clock::now();
                auto recv_time_us = std::chrono::duration_cast<std::chrono::microseconds>(recv_time_system.time_since_epoch()).count();

                std::string received_message(buf.data(), bytes_received);
                db<ECUComponent>(TRC) << "[ECU " << Component::getName() << "] Received " << bytes_received << " bytes from " << source_addr << ": " << received_message << "\n";

                // Attempt to parse the message (assuming a common format for now)
                // Example format: "[SourceType] Vehicle [ID] message [MsgID] at [Timestamp]: [Payload]"
                std::regex pattern("\\[(\w+)\\] Vehicle (\\d+) message (\\d+) at (\\d+): (.*)");
                std::smatch matches;

                long long send_time_us = 0;
                long long latency_us = -1;
                int source_vehicle_id = -1;
                int message_id = -1;
                std::string source_component_type = "unknown";
                std::string payload = received_message; // Default payload is the raw message

                if (std::regex_match(received_message, matches, pattern) && matches.size() > 5) {
                    try {
                        source_component_type = matches[1].str();
                        source_vehicle_id = std::stoi(matches[2].str());
                        message_id = std::stoi(matches[3].str());
                        send_time_us = std::stoll(matches[4].str());
                        payload = matches[5].str(); // The actual data part
                        latency_us = recv_time_us - send_time_us;

                         db<ECUComponent>(INF) << "[ECU " << Component::getName()
                                              << "] Parsed [" << source_component_type << "] msg from Vehicle " << source_vehicle_id
                                              << ", msg_id=" << message_id
                                              << ", latency=" << latency_us << "us"
                                              << ", Payload: " << payload << "\n";
                    } catch (const std::exception& e) {
                         db<ECUComponent>(ERR) << "[ECU " << Component::getName() << "] Exception parsing message: " << e.what() << " | Original: " << received_message << "\n";
                         source_component_type = "parse_error"; // Mark as parse error
                         source_vehicle_id = -1;
                         message_id = -1;
                         send_time_us = 0;
                         latency_us = -1;
                    }
                } else {
                     db<ECUComponent>(WRN) << "[ECU " << Component::getName() << "] Failed to parse message format: " << received_message << "\n";
                     source_component_type = "unknown_format"; // Mark as unknown format
                }

                // Log the received data
                if (_log_file.is_open()) {
                     std::string source_addr_str = source_addr.to_string();
                     // Basic CSV escaping for the raw message
                     std::string escaped_message = received_message;
                     std::replace(escaped_message.begin(), escaped_message.end(), ',', ';'); // Replace commas to avoid breaking CSV

                     _log_file << recv_time_us << ","
                               << source_addr_str << ","
                               << source_component_type << ","
                               << (source_vehicle_id != -1 ? std::to_string(source_vehicle_id) : "unknown") << ","
                               << (message_id != -1 ? std::to_string(message_id) : "unknown") << ",receive,"
                               << (send_time_us != 0 ? std::to_string(send_time_us) : "unknown") << ","
                               << (latency_us != -1 ? std::to_string(latency_us) : "unknown") << ","
                               << "\"" << escaped_message << "\"\n"; // Quote the message
                     _log_file.flush();
                }
            }
        }

         db<ECUComponent>(INF) << "[ECUComponent " << Component::getName() << "] thread terminated.\n";
    }

private:
    // No additional private members needed for this basic version
};

#endif // ECU_COMPONENT_H 