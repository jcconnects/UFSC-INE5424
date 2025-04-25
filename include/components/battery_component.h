#ifndef BATTERY_COMPONENT_H
#define BATTERY_COMPONENT_H

#include "component.h"
#include "vehicle.h"
#include "debug.h"
#include "ethernet.h"
#include <chrono>
#include <random>
#include <unistd.h>
#include <thread>
#include <string>
#include <sstream>
#include <iomanip> // For std::fixed, std::setprecision


// Assuming ECU2 will have port 1 based on creation order
// Note: If lidar_component.h is also included, this might be a redefinition.
extern const unsigned short ECU2_PORT; // Use extern if defined elsewhere


class BatteryComponent : public Component {
public:
    BatteryComponent(Vehicle* vehicle, const std::string& name, TheProtocol* protocol, TheAddress address)
        : Component(vehicle, name, protocol, address),
          _gen(_rd()),
          _voltage_dist(48.0, 54.0),   // Example EV battery voltage range (V)
          _current_dist(-50.0, 100.0), // Example current range (A, negative for regen)
          _temp_dist(15.0, 40.0),      // Example temperature range (C)
          _soc_dist(0.2, 1.0),         // Example State of Charge (20% - 100%)
          _delay_dist(800, 1200)       // Milliseconds delay (battery data updates less frequently)
    {
        open_log_file("battery_log");
        if (_log_file.is_open()) {
            _log_file.seekp(0);
            _log_file << "timestamp_us,source_vehicle,message_id,event_type,destination_address,voltage_v,current_a,temperature_c,soc_pct\n";
            _log_file.flush();
        }

        // Determine the local address for ECU2
        _ecu2_address = TheAddress(address.paddr(), ECU2_PORT);
         db<BatteryComponent>(INF) << Component::getName() << " targeting local ECU2 at: " << _ecu2_address << "\n";

        // Define the broadcast address
        _broadcast_address = TheAddress(Ethernet::BROADCAST, 0);
         db<BatteryComponent>(INF) << Component::getName() << " targeting broadcast at: " << _broadcast_address << "\n";
    }

    void run() override {
         db<BatteryComponent>(INF) << "[" << Component::getName() << "] thread running.\n";
        int counter = 1;

        while (running()) {
            auto now_system = std::chrono::system_clock::now();
            auto time_us_system = std::chrono::duration_cast<std::chrono::microseconds>(now_system.time_since_epoch()).count();

            // Generate dummy battery data
            double voltage = _voltage_dist(_gen);
            double current = _current_dist(_gen);
            double temp = _temp_dist(_gen);
            double soc = _soc_dist(_gen) * 100.0; // Convert to percentage for payload

            std::stringstream payload_ss;
            payload_ss << std::fixed << std::setprecision(2)
                       << "BatteryStatus: {"
                       << "Voltage: " << voltage << "V"
                       << ", Current: " << current << "A"
                       << ", Temp: " << temp << "C"
                       << ", SoC: " << soc << "%"
                       << "}";
            std::string payload = payload_ss.str();

            // Construct the full message string
            std::string msg = "[" + Component::getName() + "] Vehicle " + std::to_string(vehicle()->id()) + " message " + std::to_string(counter) + " at " + std::to_string(time_us_system) + ": " + payload;

             // Ensure message isn't too large
              if (msg.size() >= TheCommunicator::MAX_MESSAGE_SIZE) {
                   db<BatteryComponent>(ERR) << "[" << Component::getName() << "] Message " << counter << " exceeds MAX_MESSAGE_SIZE (" << msg.size() << "), skipping send.\n";
                  continue;
              }

            // 1. Send to local ECU2
             db<BatteryComponent>(TRC) << "[" << Component::getName() << "] sending msg " << counter << " to ECU2: " << _ecu2_address << "\n";
            int bytes_sent_local = send(_ecu2_address, msg.c_str(), msg.size());
             if (bytes_sent_local > 0) {
                  db<BatteryComponent>(INF) << "[" << Component::getName() << "] msg " << counter << " sent locally! (" << bytes_sent_local << " bytes)\n";
                 if (_log_file.is_open()) {
                      _log_file << time_us_system << "," << vehicle()->id() << "," << counter << ",send_local," << _ecu2_address << ","
                                << std::fixed << std::setprecision(2) << voltage << "," << current << "," << temp << "," << soc << "\n";
                      _log_file.flush();
                 }
             } else if(running()) {
                  db<BatteryComponent>(ERR) << "[" << Component::getName() << "] failed to send msg " << counter << " locally to " << _ecu2_address << "!\n";
             }


            // 2. Send to broadcast address
             db<BatteryComponent>(TRC) << "[" << Component::getName() << "] broadcasting msg " << counter << " to " << _broadcast_address << "\n";
            int bytes_sent_bcast = send(_broadcast_address, msg.c_str(), msg.size());
             if (bytes_sent_bcast > 0) {
                  db<BatteryComponent>(INF) << "[" << Component::getName() << "] msg " << counter << " broadcast! (" << bytes_sent_bcast << " bytes)\n";
                 // Optional: Log broadcast send
             } else if(running()) {
                  db<BatteryComponent>(ERR) << "[" << Component::getName() << "] failed to broadcast msg " << counter << "!\n";
             }

            counter++;

            // Wait for a delay
            int wait_time_ms = _delay_dist(_gen);
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_time_ms));
        }

         db<BatteryComponent>(INF) << "[" << Component::getName() << "] thread terminated.\n";
    }

private:
    TheAddress _ecu2_address;
    TheAddress _broadcast_address;

    // Random number generation
    std::random_device _rd;
    std::mt19937 _gen;
    std::uniform_real_distribution<> _voltage_dist;
    std::uniform_real_distribution<> _current_dist;
    std::uniform_real_distribution<> _temp_dist;
    std::uniform_real_distribution<> _soc_dist;
    std::uniform_int_distribution<> _delay_dist;
};

#endif // BATTERY_COMPONENT_H 