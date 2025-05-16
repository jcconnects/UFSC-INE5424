#ifndef TEST_CONSUMER_H
#define TEST_CONSUMER_H

#include <chrono>
#include <string>
#include <mutex>
#include <vector>

#include "component.h"
#include "debug.h"
#include "teds.h"
#include "message.h"

class BasicConsumer : public Component {
    public:
        // Define the port
        static const unsigned int PORT = 106; // Unique port for Basic Consumer

        // Constructor
        BasicConsumer(Vehicle* vehicle, const unsigned int vehicle_id, const std::string& name, 
                     VehicleProt* protocol);

        ~BasicConsumer() = default;

        // The main loop for the consumer component
        void run() override;

    private:
        // Interest period for requesting test data (in microseconds)
        inline static const std::uint32_t TEST_DATA_PERIOD_US = 500000; // 500ms
        
        // Latest received test data
        struct {
            int value = 0;
            uint32_t counter = 0;
            bool data_valid = false;
            std::chrono::time_point<std::chrono::high_resolution_clock> last_update;
            std::mutex mutex;
        } _latest_test_data;
        
        // Handler for received test data messages
        void handle_test_data(const Message& message);
        
        // Parse test data from message
        bool parse_test_data(const Message& message, int& out_value, uint32_t& out_counter);
};

/*********** Basic Consumer Implementation **********/
BasicConsumer::BasicConsumer(Vehicle* vehicle, const unsigned int vehicle_id, const std::string& name, 
                           VehicleProt* protocol) 
    : Component(vehicle, vehicle_id, name, ComponentType::CONSUMER) 
{
    // Sets CSV result Header
    open_log_file();
    if (_log_file.is_open()) {
        _log_file.seekp(0); // Go to beginning to overwrite if file exists
        // Define log header
        _log_file << "timestamp_us,received_value,processed_value,counter\n";
        _log_file.flush();
    }

    // Sets own address
    Address addr(_vehicle->address(), PORT);

    // Set up communicator as a CONSUMER (without setting interest - will be done in run())
    _communicator = new Comms(protocol, addr, ComponentType::CONSUMER, DataTypeId::UNKNOWN);
    set_address(addr);

    db<BasicConsumer>(INF) << "[Basic Consumer] initialized, will register interest in CUSTOM_SENSOR_DATA_A\n";
}

void BasicConsumer::run() {
    db<BasicConsumer>(INF) << "[Basic Consumer] component " << getName() << " starting main run loop.\n";
    
    // Register interest in test data with callback
    register_interest(
        DataTypeId::CUSTOM_SENSOR_DATA_A, 
        TEST_DATA_PERIOD_US,
        [this](const Message& msg) { 
            db<BasicConsumer>(INF) << "[Basic Consumer] Interest callback called for message\n";
            this->handle_test_data(msg);
        }
    );
    
    db<BasicConsumer>(INF) << "[Basic Consumer] registered interest in CUSTOM_SENSOR_DATA_A with period " 
                         << TEST_DATA_PERIOD_US << " microseconds\n";
    
    // Main loop - process the latest data periodically
    while (running()) {
        // Get latest test data
        int current_value = 0;
        uint32_t current_counter = 0;
        bool data_valid = false;
        
        {
            std::lock_guard<std::mutex> lock(_latest_test_data.mutex);
            
            data_valid = _latest_test_data.data_valid;
            if (data_valid) {
                current_value = _latest_test_data.value;
                current_counter = _latest_test_data.counter;
            }
        }
        
        // Process and display data if valid
        if (data_valid) {
            // Add 5 to the received value as requested
            int processed_value = current_value + 5;
            
            // Log current data
            if (_log_file.is_open()) {
                auto now = std::chrono::high_resolution_clock::now();
                auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                    now.time_since_epoch()).count();
                    
                _log_file << now_us << ","
                         << current_value << ","
                         << processed_value << ","
                         << current_counter << "\n";
                _log_file.flush();
            }
            
            // Print the processed value
            db<BasicConsumer>(INF) << "[Basic Consumer] " << getName() << " received value=" 
                                 << current_value << ", processed value="
                                 << processed_value << ", counter=" 
                                 << current_counter << "\n";
        }
        
        // Sleep to prevent consuming too much CPU
        // Use a sleep duration that matches our interest period for consistency
        usleep(TEST_DATA_PERIOD_US / 2); // Sleep for half the interest period
    }
    
    db<BasicConsumer>(INF) << "[Basic Consumer] component " << getName() << " exiting main run loop.\n";
}

void BasicConsumer::handle_test_data(const Message& message) {
    db<BasicConsumer>(INF) << "[Basic Consumer] handle_test_data() called with message type " 
                         << static_cast<int>(message.message_type()) 
                         << " and unit type " << static_cast<int>(message.unit_type()) << "\n";
    
    // Parse the test data from the message
    int value;
    uint32_t counter;
    if (!parse_test_data(message, value, counter)) {
        db<BasicConsumer>(WRN) << "[Basic Consumer] received invalid test data message\n";
        return;
    }
    
    // Update the latest test data
    {
        std::lock_guard<std::mutex> lock(_latest_test_data.mutex);
        _latest_test_data.value = value;
        _latest_test_data.counter = counter;
        _latest_test_data.data_valid = true;
        _latest_test_data.last_update = std::chrono::high_resolution_clock::now();
    }
    
    db<BasicConsumer>(INF) << "[Basic Consumer] Received data: value=" 
                         << value << ", counter=" << counter << "\n";
}

bool BasicConsumer::parse_test_data(const Message& message, int& out_value, uint32_t& out_counter) {
    db<BasicConsumer>(TRC) << "[Basic Consumer] parse_test_data() called\n";
    
    // Verify this is the right message type and has valid data
    if (message.message_type() != Message::Type::RESPONSE || 
        message.unit_type() != DataTypeId::CUSTOM_SENSOR_DATA_A) {
        db<BasicConsumer>(WRN) << "[Basic Consumer] parse_test_data() received invalid message type " 
                             << static_cast<int>(message.message_type()) 
                             << " or unit type " << static_cast<int>(message.unit_type()) << "\n";
        return false;
    }
    
    // Get the message value data
    const void* value_data = message.value();
    std::size_t value_size = message.value_size();
    
    // Check size matches our expected data (int + uint32_t)
    if (!value_data || value_size < sizeof(int) + sizeof(uint32_t)) {
        db<BasicConsumer>(WRN) << "[Basic Consumer] parse_test_data() received invalid message data: " 
                             << (value_data ? "data not null, " : "data null, ")
                             << "size=" << value_size << ", expected at least " 
                             << (sizeof(int) + sizeof(uint32_t)) << " bytes\n";
        return false;
    }
    
    // Copy the data
    std::memcpy(&out_value, value_data, sizeof(int));
    std::memcpy(&out_counter, static_cast<const uint8_t*>(value_data) + sizeof(int), sizeof(uint32_t));
    
    return true;
}

#endif // TEST_CONSUMER_H
