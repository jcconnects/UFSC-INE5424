#ifndef TEST_PRODUCER_H
#define TEST_PRODUCER_H

#include <chrono>
#include <string>
#include <random>
#include <vector>
#include <atomic>

#include "component.h"
#include "debug.h"
#include "teds.h"

class BasicProducer : public Component {
    public:
        // Define the port
        static const unsigned int PORT;

        BasicProducer(Vehicle* vehicle, const unsigned int vehicle_id, 
                    const std::string& name, VehicleProt* protocol);

        ~BasicProducer() = default;

        void run() override;
        
    protected:
        // Override produce_data_for_response to generate test data
        bool produce_data_for_response(DataTypeId type, std::vector<std::uint8_t>& out_value) override;
        
    private:
        // Random number generator for simulated data
        std::mt19937 _rng;
        std::uniform_int_distribution<int> _value_dist; // Value distribution
        
        // Current data
        int _current_value;
        uint32_t _counter;
        
        // Update the simulated test data
        void update_test_data();
};

/******** Test Producer Implementation *******/
const unsigned int BasicProducer::PORT = 105; // Unique port for Test Producer

BasicProducer::BasicProducer(Vehicle* vehicle, const unsigned int vehicle_id, 
                           const std::string& name, VehicleProt* protocol) 
    : Component(vehicle, vehicle_id, name), 
      _rng(std::random_device{}()),
      _value_dist(0, 1000),   // 0 to 1000 range
      _current_value(0),
      _counter(0)
{
    // Initialize as a producer of CUSTOM_SENSOR_DATA_A data type
    _produced_data_type = DataTypeId::CUSTOM_SENSOR_DATA_A;
    
    // Set up logging
    open_log_file();
    if (_log_file.is_open()) {
        _log_file.seekp(0);
        _log_file << "timestamp_us,value,counter\n";
        _log_file.flush();
    }

    // Initialize with random test data
    update_test_data();
    
    // Set up communicator with test producer port
    Address addr(_vehicle->address(), PORT);
    _communicator = new Comms(protocol, addr, ComponentType::PRODUCER, DataTypeId::CUSTOM_SENSOR_DATA_A);
    
    db<BasicProducer>(INF) << "[Basic Producer] initialized as producer of type " 
                         << static_cast<int>(_produced_data_type) << "\n";
}

void BasicProducer::run() {
    db<BasicProducer>(INF) << "[Basic Producer] component " << getName() << " starting main run loop.\n";
    
    // With hardcoded approach, we don't need to send registration messages
    // The Gateway knows about us through the Vehicle's producer map
    
    // Start response thread immediately since we're considered always registered
    start_producer_response_thread();
    db<BasicProducer>(INF) << "[Basic Producer] started response thread automatically\n";
    
    // Main loop - update sensor data periodically
    while (running()) {
        // Update simulated test data
        update_test_data();
        
        // Log current data
        if (_log_file.is_open()) {
            auto now = std::chrono::high_resolution_clock::now();
            auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                now.time_since_epoch()).count();
                
            _log_file << now_us << ","
                     << _current_value << ","
                     << _counter << "\n";
            _log_file.flush();
        }
        
        // Sleep for a bit - actual response rate is determined by consumer periods
        usleep(100000); // 100ms update interval
    }
    
    db<BasicProducer>(INF) << "[Basic Producer] component " << getName() << " exiting main run loop.\n";
}

void BasicProducer::update_test_data() {
    // Generate new simulated data
    _current_value = _value_dist(_rng);
    _counter++;
    
    db<BasicProducer>(TRC) << "[Basic Producer] updated data: value=" 
                         << _current_value << ", counter="
                         << _counter << "\n";
}

bool BasicProducer::produce_data_for_response(DataTypeId type, std::vector<std::uint8_t>& out_value) {
    db<BasicProducer>(TRC) << "[Basic Producer] produce_data_for_response called for type " << static_cast<int>(type) << ". Expected: " << static_cast<int>(DataTypeId::CUSTOM_SENSOR_DATA_A) << "\n";
    // Only respond to requests for our produced data type
    if (type != DataTypeId::CUSTOM_SENSOR_DATA_A) {
        db<BasicProducer>(WRN) << "[Basic Producer] produce_data_for_response rejected due to type mismatch.\n";
        return false;
    }
    
    // Create a simple buffer with our two values
    out_value.resize(sizeof(int) + sizeof(uint32_t));
    std::memcpy(out_value.data(), &_current_value, sizeof(int));
    std::memcpy(out_value.data() + sizeof(int), &_counter, sizeof(uint32_t));
    
    db<BasicProducer>(INF) << "[Basic Producer] produced data (value=" << _current_value << ", counter=" << _counter << ") with size " << out_value.size() << " bytes for type " << static_cast<int>(type) << "\n";
    return true;
}

#endif // TEST_PRODUCER_H
