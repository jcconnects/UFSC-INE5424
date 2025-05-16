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
        static inline const unsigned int PORT = 105;

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

/******** Basic Producer Implementation *******/
// const unsigned int BasicProducer::PORT = 105; // This line is removed

BasicProducer::BasicProducer(Vehicle* vehicle, const unsigned int vehicle_id, 
                           const std::string& name, VehicleProt* protocol) 
    : Component(vehicle, vehicle_id, name, ComponentType::PRODUCER), 
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
    
    // Set up communicator
    Address addr(_vehicle->address(), PORT);
    _communicator = new Comms(protocol, addr, ComponentType::PRODUCER, _produced_data_type);
    set_address(addr);
    
    db<BasicProducer>(INF) << "[Basic Producer] initialized as producer of type " 
                         << static_cast<int>(_produced_data_type) << "\n";
}

void BasicProducer::run() {
    db<BasicProducer>(INF) << "[Basic Producer] component " << getName() << " starting main run loop.\n";
    
    // Main loop - update data and log
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
        
        // Sleep to prevent consuming too much CPU
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
    // Only respond to requests for our produced data type
    if (type != _produced_data_type) {
        return false;
    }
    
    // Create a buffer with our two values
    out_value.resize(sizeof(int) + sizeof(uint32_t));
    std::memcpy(out_value.data(), &_current_value, sizeof(int));
    std::memcpy(out_value.data() + sizeof(int), &_counter, sizeof(uint32_t));
    
    db<BasicProducer>(INF) << "[Basic Producer] produced data: value=" << _current_value 
                         << ", counter=" << _counter << "\n";
    return true;
}

#endif // TEST_PRODUCER_H
