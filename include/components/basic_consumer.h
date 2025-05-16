#ifndef TEST_CONSUMER_H
#define TEST_CONSUMER_H

#include <chrono>
#include <string>
#include <mutex>
#include <vector>

// Forward declarations and minimal includes
#include "debug.h"
#include "teds.h"
#include "message.h"
#include "ethernet.h"

// Forward declarations
class Vehicle;
class SocketEngine;
class SharedMemoryEngine;

template <typename Engine1, typename Engine2>
class NIC;

template <typename NIC>
class Protocol;

class Component;

class BasicConsumer : public Component {
    public:
        // Define the port
        static const unsigned int PORT = 106; // Unique port for Test Consumer

        // Basic constructor
        BasicConsumer(Vehicle* vehicle, const unsigned int vehicle_id, const std::string& name, 
                      Protocol<NIC<SocketEngine, SharedMemoryEngine>>* protocol);

        ~BasicConsumer() = default;

        // The main loop for the consumer component thread
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
        
        // Helper method to log received messages
        void log_message(const Message& message, const std::chrono::microseconds& recv_time, 
                        const std::chrono::microseconds& timestamp, const std::string& message_details);
};

// Now include component.h for the implementation
#include "component.h"

/*********** Test Consumer Implementation **********/
BasicConsumer::BasicConsumer(Vehicle* vehicle, const unsigned int vehicle_id, const std::string& name, 
                           Protocol<NIC<SocketEngine, SharedMemoryEngine>>* protocol) 
    : Component(vehicle, vehicle_id, name) 
{
    // Sets CSV result Header
    open_log_file();
    if (_log_file.is_open()) {
        _log_file.seekp(0); // Go to beginning to overwrite if file exists
        // Define log header
        _log_file << "timestamp_us,received_value,received_counter,validity\n";
        _log_file.flush();
    }

    // Sets own address
    Address addr(_vehicle->address(), PORT);

    // Sets own communicator
    _communicator = new Comms(protocol, addr, ComponentType::CONSUMER, DataTypeId::CUSTOM_SENSOR_DATA_A);
    set_address(addr);

    db<BasicConsumer>(INF) << "[Basic Consumer] initialized, ready to register interests\n";
}

void BasicConsumer::run() {
    db<BasicConsumer>(INF) << "[Basic Consumer] component " << getName() << " starting main run loop.\n";
    
    // No need to delay for producer registration since we use hardcoded components

    // Register interest in test data
    register_interest_handler(
        DataTypeId::CUSTOM_SENSOR_DATA_A, 
        TEST_DATA_PERIOD_US,
        [this](const Message& msg) { 
            db<BasicConsumer>(INF) << "[Basic Consumer] Interest handler lambda called for message\n";
            this->handle_test_data(msg);
        }
    );
    
    db<BasicConsumer>(INF) << "[Basic Consumer] registered interest in CUSTOM_SENSOR_DATA_A with period " 
                         << TEST_DATA_PERIOD_US << " microseconds\n";
    
    // Main loop - process and display received data periodically
    while (running()) {
        // Get latest test data
        int current_value = 0;
        uint32_t current_counter = 0;
        bool data_valid = false;
        std::chrono::microseconds data_age(0);
        
        {
            std::lock_guard<std::mutex> lock(_latest_test_data.mutex);
            
            data_valid = _latest_test_data.data_valid;
            if (data_valid) {
                current_value = _latest_test_data.value;
                current_counter = _latest_test_data.counter;
                
                auto now = std::chrono::high_resolution_clock::now();
                data_age = std::chrono::duration_cast<std::chrono::microseconds>(
                    now - _latest_test_data.last_update);
            }
        }
        
        // Log current data
        if (_log_file.is_open()) {
            auto now = std::chrono::high_resolution_clock::now();
            auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                now.time_since_epoch()).count();
                
            _log_file << now_us << ",";
            
            if (data_valid) {
                _log_file << current_value << ","
                         << current_counter << ","
                         << "valid";
            } else {
                _log_file << "0,0,invalid";
            }
            
            _log_file << "\n";
            _log_file.flush();
        }
        
        // Display current status
        if (data_valid) {
            db<BasicConsumer>(INF) << "[Basic Consumer] " << getName() << " processed test data: value=" 
                                 << current_value << ", counter=" 
                                 << current_counter
                                 << ", age=" << data_age.count() << "us\n";
            
            // Example of acting on the data
            if (current_value > 800) {
                db<BasicConsumer>(INF) << "[Basic Consumer] " << getName() << " ALERT: High value detected!\n";
            }
        } else {
            db<BasicConsumer>(INF) << "[Basic Consumer] " << getName() << " waiting for test data...\n";
        }
        
        // Sleep a bit
        usleep(500000); // 500ms
    }
    
    db<BasicConsumer>(INF) << "[Basic Consumer] component " << getName() << " exiting main run loop.\n";
}

void BasicConsumer::handle_test_data(const Message& message) {
    db<BasicConsumer>(INF) << "[Basic Consumer] handle_test_data() called with message type " 
                         << static_cast<int>(message.message_type()) 
                         << " and unit type " << static_cast<int>(message.unit_type()) << "\n";
    
    // Log the raw message details
    std::size_t value_size = message.value_size();
    
    db<BasicConsumer>(INF) << "[Basic Consumer] Received message value_size=" << value_size
                         << ", from=" << message.origin().to_string() << "\n";
    
    // Parse the test data from the message
    int value;
    uint32_t counter;
    if (!parse_test_data(message, value, counter)) {
        db<BasicConsumer>(WRN) << "[Basic Consumer] received invalid test data message\n";
        return;
    }
    
    db<BasicConsumer>(INF) << "[Basic Consumer] Successfully parsed test data: value=" 
                         << value << ", counter=" << counter << "\n";
    
    // Update the latest test data
    {
        std::lock_guard<std::mutex> lock(_latest_test_data.mutex);
        _latest_test_data.value = value;
        _latest_test_data.counter = counter;
        _latest_test_data.data_valid = true;
        _latest_test_data.last_update = std::chrono::high_resolution_clock::now();
    }
    
    // Log the received message
    auto now = std::chrono::high_resolution_clock::now();
    auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
    
    std::string details = "value=" + std::to_string(value) + 
                         ", counter=" + std::to_string(counter);
    
    log_message(message, now_us, std::chrono::microseconds(message.timestamp()), details);
    
    db<BasicConsumer>(TRC) << "[Basic Consumer] updated test data from message\n";
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
    
    db<BasicConsumer>(TRC) << "[Basic Consumer] parse_test_data() successfully extracted value=" 
                         << out_value << ", counter=" << out_counter << "\n";
    return true;
}

void BasicConsumer::log_message(const Message& message, const std::chrono::microseconds& recv_time,
                              const std::chrono::microseconds& timestamp, const std::string& message_details) {
    auto message_type = message.message_type();
    auto origin = message.origin();
    auto type_id = message.unit_type();
    auto latency_us = recv_time.count() - timestamp.count();
    
    // Log detailed debug info
    std::string message_type_str = (message_type == Message::Type::INTEREST) ? "INTEREST" : 
                                  (message_type == Message::Type::RESPONSE) ? "RESPONSE" : "OTHER";
    
    db<BasicConsumer>(TRC) << "[Basic Consumer] " << getName() << " received " << message_type_str 
                         << " from " << origin.to_string()
                         << " for type " << static_cast<int>(type_id)
                         << " with latency " << latency_us << "us\n";
}

#endif // TEST_CONSUMER_H
