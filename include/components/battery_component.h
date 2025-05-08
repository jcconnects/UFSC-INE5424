#ifndef BATTERY_COMPONENT_H
#define BATTERY_COMPONENT_H

#include <chrono>
#include <random>
#include <string>
#include <vector>
#include <iomanip> // For std::fixed, std::setprecision

#include "component.h"
#include "debug.h"
#include "teds.h"


class BatteryComponent : public Component {
    public:
        static const unsigned int PORT;
        
        // Battery data structure
        struct BatteryStatusData {
            float voltage_v;
            float current_a;
            float temperature_c;
            float soc_percent;  // State of Charge percentage
            
            // Serialization helper
            static std::vector<std::uint8_t> serialize(const BatteryStatusData& data) {
                std::vector<std::uint8_t> result(sizeof(BatteryStatusData));
                std::memcpy(result.data(), &data, sizeof(BatteryStatusData));
                return result;
            }
            
            // Deserialization helper
            static BatteryStatusData deserialize(const std::vector<std::uint8_t>& bytes) {
                BatteryStatusData data;
                if (bytes.size() >= sizeof(BatteryStatusData)) {
                    std::memcpy(&data, bytes.data(), sizeof(BatteryStatusData));
                }
                return data;
            }
        };

        BatteryComponent(Vehicle* vehicle, const unsigned int vehicle_id, const std::string& name, VehicleProt* protocol);

        ~BatteryComponent() = default;

        void run() override;
        
    protected:
        // Override produce_data_for_response to generate battery data
        bool produce_data_for_response(DataTypeId type, std::vector<std::uint8_t>& out_value) override;

    private:
        // Random number generation
        std::mt19937 _gen;
        std::uniform_real_distribution<> _voltage_dist;
        std::uniform_real_distribution<> _current_dist;
        std::uniform_real_distribution<> _temp_dist;
        std::uniform_real_distribution<> _soc_dist;
        
        // Current battery status data
        BatteryStatusData _current_data;
        
        // Update the simulated battery data
        void update_battery_data();
};

/************* BatteryComponent Implementation ***************/
const unsigned int BatteryComponent::PORT = 103; // Example port for Battery

BatteryComponent::BatteryComponent(Vehicle* vehicle, const unsigned int vehicle_id, const std::string& name, VehicleProt* protocol) 
    : Component(vehicle, vehicle_id, name),
    _gen(std::random_device{}()),
    _voltage_dist(48.0, 54.0),   // Example EV battery voltage range (V)
    _current_dist(-50.0, 100.0), // Example current range (A, negative for regen)
    _temp_dist(15.0, 40.0),      // Example temperature range (C)
    _soc_dist(0.2, 1.0)          // Example State of Charge (20% - 100%)
{
    // Initialize as a producer of VEHICLE_SPEED data
    _produced_data_type = DataTypeId::ENGINE_RPM;
    
    db<BatteryComponent>(TRC) << "BatteryComponent::BatteryComponent() called!\n";

    // Sets CSV result header
    open_log_file();
    if (_log_file.is_open()) {
        _log_file.seekp(0);
        _log_file << "timestamp_us,voltage_v,current_a,temperature_c,soc_pct\n";
        _log_file.flush();
    }

    // Initialize with a random battery data
    update_battery_data();

    // Sets own address
    Address addr(_vehicle->address().paddr(), PORT);

    // Sets communicator
    _communicator = new Comms(protocol, addr);
    if (_communicator) {
        _communicator->set_owner_component(this);
    }
    
    db<BatteryComponent>(INF) << "Battery Component initialized as producer of type " 
                             << static_cast<int>(_produced_data_type) << "\n";
}

void BatteryComponent::run() {
    db<BatteryComponent>(INF) << "Battery component " << getName() << " starting main run loop.\n";
    
    // Send REG_PRODUCER message to Gateway
    Message reg_msg = _communicator->new_message(
        Message::Type::REG_PRODUCER,
        _produced_data_type
    );
    
    // Send to Gateway (port 0)
    Address gateway_addr(_vehicle->address().paddr(), 0);
    _communicator->send(reg_msg, gateway_addr);
    
    db<BatteryComponent>(INF) << "Battery sent REG_PRODUCER for type " 
                             << static_cast<int>(_produced_data_type) 
                             << " to Gateway.\n";
    
    // Main loop - update sensor data periodically
    // Note: The actual periodic response sending is handled by producer_response_thread
    while (running()) {
        // Update simulated battery data
        update_battery_data();
        
        // Log current data
        if (_log_file.is_open()) {
            auto now = std::chrono::high_resolution_clock::now();
            auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                now.time_since_epoch()).count();
                
            _log_file << now_us << ","
                     << _current_data.voltage_v << ","
                     << _current_data.current_a << ","
                     << _current_data.temperature_c << ","
                     << _current_data.soc_percent << "\n";
            _log_file.flush();
        }
        
        // Sleep for a bit - actual response rate is determined by consumer periods
        usleep(1000000); // 1 second
    }
    
    db<BatteryComponent>(INF) << "Battery component " << getName() << " exiting main run loop.\n";
}

void BatteryComponent::update_battery_data() {
    // Generate new simulated data
    _current_data.voltage_v = _voltage_dist(_gen);
    _current_data.current_a = _current_dist(_gen);
    _current_data.temperature_c = _temp_dist(_gen);
    _current_data.soc_percent = _soc_dist(_gen) * 100.0; // Convert 0-1 to percentage
    
    db<BatteryComponent>(TRC) << "Battery updated status data: voltage=" 
                             << _current_data.voltage_v << "V, current="
                             << _current_data.current_a << "A, temp="
                             << _current_data.temperature_c << "°C, SoC="
                             << _current_data.soc_percent << "%\n";
}

bool BatteryComponent::produce_data_for_response(DataTypeId type, std::vector<std::uint8_t>& out_value) {
    // Only respond to requests for our produced data type
    if (type != DataTypeId::ENGINE_RPM) {
        return false;
    }
    
    // Serialize the current battery data
    out_value = BatteryStatusData::serialize(_current_data);
    
    db<BatteryComponent>(INF) << "Battery produced data with size " << out_value.size() << " bytes\n";
    return true;
}

#endif // BATTERY_COMPONENT_H 