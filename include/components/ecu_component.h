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
        // ECU receives a port for identification
        ECUComponent(Vehicle* vehicle, const unsigned int vehicle_id, const std::string& name, VehicleProt* protocol, Vehicle::Ports port = Vehicle::Ports::ECU1);

        ~ECUComponent() = default;

        // The main loop for the ECU component thread
        void run() override;

    private:
        // No additional private members needed for this basic version
};

/*********** ECU Component Implementation **********/
ECUComponent::ECUComponent(Vehicle* vehicle, const unsigned int vehicle_id, const std::string& name, VehicleProt* protocol, Vehicle::Ports port) : Component(vehicle, vehicle_id, name) 
{
    // Sets CSV result Header
    open_log_file();
    if (_log_file.is_open()) {
        _log_file.seekp(0); // Go to beginning to overwrite if file exists
        // Define log header
        _log_file << "receive_timestamp_us,source_address,source_component_type,source_vehicle,message_id,event_type,send_timestamp_us,latency_us,raw_message\n";
        _log_file.flush();
    }

    // Sets own address
    Address addr(_vehicle->address(), static_cast<unsigned int>(port));

    // Sets own communicator
    _communicator = new Comms(protocol, addr);
}

void ECUComponent::run() {
    db<ECUComponent>(INF) << "[ECUComponent] " << Component::getName() << " thread running.\n";

    while (running()) {
        
        unsigned int size = Comms::MAX_MESSAGE_SIZE;
        char buf[size];
        Address source_addr;

        int bytes_received = receive(buf, size, &source_addr);

        if (bytes_received) {
            auto recv_time_system = std::chrono::system_clock::now();
            auto recv_time_us = std::chrono::duration_cast<std::chrono::microseconds>(recv_time_system.time_since_epoch()).count();

            std::string received_message(buf, bytes_received);
            db<ECUComponent>(TRC) << "[ECUComponent] " << Component::getName() << " received " << bytes_received << " bytes from " << source_addr.to_string() << ": " << received_message << "\n";

            // Attempt to parse the message (assuming a common format for now)
            // Example format: "[SourceType] Vehicle [ID] message [MsgID] at [Timestamp]: [Payload]"
            std::regex pattern("\\[(\\w+)\\] Vehicle (\\d+) message (\\d+) at (\\d+): (.*)");
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

                    db<ECUComponent>(INF) << "[ECUComponent] " << Component::getName()
                                        << " Parsed [" << source_component_type << "] message from Vehicle " << source_vehicle_id
                                        << ", msg_id=" << message_id
                                        << ", latency=" << latency_us << "us"
                                        << ", Payload: " << payload << "\n";
                } catch (const std::exception& e) {
                    db<ECUComponent>(ERR) << "[ECUComponent] " << Component::getName() << " exception thrown while parsing message: " << e.what() << " | Original: " << received_message << "\n";
                    source_component_type = "parse_error"; // Mark as parse error
                    source_vehicle_id = -1;
                    message_id = -1;
                    send_time_us = 0;
                    latency_us = -1;
                }
            } else {
                db<ECUComponent>(WRN) << "[ECUComponent] " << Component::getName() << " failed to parse message format: " << received_message << "\n";
                source_component_type = "unknown_format"; // Mark as unknown format
            }

            // Log the received data
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

    db<ECUComponent>(INF) << "[ECUComponent] " << Component::getName() << " thread terminated.\n";
}
#endif // ECU_COMPONENT_H 