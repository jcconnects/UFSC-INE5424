#ifndef BATTERY_COMPONENT_H
#define BATTERY_COMPONENT_H

#include <chrono>
#include <random>
#include <unistd.h>
#include <thread>
#include <string>
#include <sstream>
#include <iomanip> // For std::fixed, std::setprecision

#include "component.h"
#include "debug.h"


class BatteryComponent : public Component {
    public:
        static const unsigned int PORT;

        BatteryComponent(Vehicle* vehicle, const unsigned int vehicle_id, const std::string& name, VehicleProt* protocol);

        ~BatteryComponent() override = default;

        void run() override;

    private:
        // Random number generation
        std::random_device _rd;
        std::mt19937 _gen;
        std::uniform_real_distribution<> _voltage_dist;
        std::uniform_real_distribution<> _current_dist;
        std::uniform_real_distribution<> _temp_dist;
        std::uniform_real_distribution<> _soc_dist;
        std::uniform_int_distribution<> _delay_dist;
};

/************* BatteryComponent Implementation ***************/
const unsigned int BatteryComponent::PORT = static_cast<unsigned int>(Vehicle::Ports::BATTERY);

BatteryComponent::BatteryComponent(Vehicle* vehicle, const unsigned int vehicle_id, const std::string& name, VehicleProt* protocol) : Component(vehicle, vehicle_id, name),
    _gen(_rd()),
    _voltage_dist(48.0, 54.0),   // Example EV battery voltage range (V)
    _current_dist(-50.0, 100.0), // Example current range (A, negative for regen)
    _temp_dist(15.0, 40.0),      // Example temperature range (C)
    _soc_dist(0.2, 1.0),         // Example State of Charge (20% - 100%)
    _delay_dist(800, 1200)       // Milliseconds delay (battery data updates less frequently)
{
    db<BatteryComponent>(TRC) << "BatteryComponent::BatteryComponent() called!\n";

    // Sets CSV result header
    open_log_file();
    if (_log_file.is_open()) {
        _log_file.seekp(0);
        _log_file << "timestamp_us,source_vehicle,message_id,event_type,destination_address,voltage_v,current_a,temperature_c,soc_pct\n";
        _log_file.flush();
    }

    // Sets own address
    Address addr(_vehicle->address(), BatteryComponent::PORT);

    // Sets communicator
    _communicator = new Comms(protocol, addr);
}

void BatteryComponent::run() {
    db<BatteryComponent>(INF) << "[BatteryComponent] " << Component::getName() << " thread running.\n";

    // Message counter
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

        // 1. Send to local ECU2
        Address ecu2_address(_vehicle->address(), static_cast<unsigned int>(Vehicle::Ports::ECU2));

        db<BatteryComponent>(INF) << "[BatteryComponent] " << Component::getName() << " sending message " << counter << " to ECU2: " << ecu2_address.to_string() << "\n";
        int bytes_sent_local = send(msg.c_str(), msg.size(), ecu2_address);

        if (bytes_sent_local > 0) {
            db<BatteryComponent>(INF) << "[BatteryComponent] " << Component::getName() << " message " << counter << " sent locally! (" << bytes_sent_local << " bytes)\n";
            
            // File is already open (on constructor)
            _log_file << time_us_system << "," << vehicle()->id() << "," << counter << ",send_local," << ecu2_address.to_string() << "," << std::fixed << std::setprecision(2) << voltage << "," << current << "," << temp << "," << soc << "\n";
            _log_file.flush();
            
        } else if(running()) {
            db<BatteryComponent>(ERR) << "[BatteryComponent] " << Component::getName() << " failed to send message " << counter << " locally to " << ecu2_address.to_string() << "!\n";
        }


        // 2. Send to broadcast address
        db<BatteryComponent>(INF) << "[BatteryComponent] " << Component::getName() << " broadcasting message " << counter << ".\n";
        int bytes_sent_bcast = send(msg.c_str(), msg.size());

        if (bytes_sent_bcast > 0) {
            db<BatteryComponent>(INF) << "[BatteryComponent] " << Component::getName() << " message " << counter << " broadcasted! (" << bytes_sent_bcast << " bytes)\n";
            // Optional: Log broadcast send
        } else if(running()) {
            db<BatteryComponent>(ERR) << "[BatteryComponent] " << Component::getName() << " failed to broadcast message " << counter << "!\n";
        }

        counter++;

        // Wait for a delay
        int wait_time_ms = _delay_dist(_gen);
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_time_ms));
    }

    db<BatteryComponent>(INF) << "[BatteryComponent] " << Component::getName() << " thread terminated.\n";
}

#endif // BATTERY_COMPONENT_H 