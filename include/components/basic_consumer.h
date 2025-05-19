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
        // Define new standardized log header
        _log_file << "timestamp_us,event_category,event_type,message_id,message_type,data_type_id,origin_address,destination_address,period_us,value_size,notes\n";
        _log_file.flush();
    }

    // Sets own address
    Address addr(_vehicle->address(), PORT);

    // Set up communicator as a CONSUMER, passing 'this' to the Communicator constructor
    _communicator = new Comms(protocol, addr, ComponentType::CONSUMER, DataTypeId::UNKNOWN);
    
    // Set the response handler callback to direct to handle_test_data
    _communicator->set_response_handler_callback([this](const Message& msg) {
        db<BasicConsumer>(TRC) << "[Basic Consumer] Response handler callback triggered for message from " << msg.origin().to_string() << "\n";
        this->handle_test_data(msg);
    });
    
    set_address(addr);

    db<BasicConsumer>(INF) << "[Basic Consumer] initialized, will register interest in CUSTOM_SENSOR_DATA_A\n";

    // Register interest in test data with callback (Moved from run() to constructor)
    register_interest(
        DataTypeId::CUSTOM_SENSOR_DATA_A, 
        TEST_DATA_PERIOD_US,
        [this](const Message& msg) { 
            db<BasicConsumer>(TRC) << "[Basic Consumer] Interest data callback triggered for message from " << msg.origin().to_string() << "\n";
            this->handle_test_data(msg);
        }
    );
    
    db<BasicConsumer>(INF) << "[Basic Consumer] Registered interest in CUSTOM_SENSOR_DATA_A with period " 
                         << TEST_DATA_PERIOD_US << " microseconds during construction.\n";
}

void BasicConsumer::run() {
    db<BasicConsumer>(INF) << "[Basic Consumer] component " << getName() << " starting main run loop.\n";
    
    // Main loop - process incoming messages and act on latest data
    while (running()) {
        Message incoming_msg = _communicator->new_message(Message::Type::RESPONSE, DataTypeId::UNKNOWN); // Prepare a message object
        int received_bytes = receive(&incoming_msg); // Blocking call, processes via Communicator::update then Component::receive
        
        // The actual data handling and logging of RECV_RESPONSE is now in handle_test_data via the callback
        // This loop primarily ensures the component stays alive and responsive if direct receive calls were used
        // or for other periodic tasks if any.
        // The old data processing and logging that was here:
        // if (data_valid) { ... log current_value, processed_value ... }
        // has been effectively replaced by the callback-driven approach for communication events.
        // We will remove the old CSV logging from here that logged processed values.

        // If direct receive was used and populated incoming_msg, handle_test_data would be called here.
        // However, with the callback model, handle_test_data is invoked directly when a relevant message is passed by Communicator.
        // So, the explicit call to handle_test_data(incoming_msg) after receive() might be redundant if the callback covers all cases.
        // For now, let's assume the callback mechanism is primary for P3.
        // If `receive()` here *does* unblock with a message not handled by the callback path (which it shouldn't with current P3 logic),
        // then `handle_test_data(incoming_msg)` would be relevant.
        if (received_bytes > 0) {
            // This path might be less common now if callbacks handle everything via Communicator::update.
            // However, if receive() itself unblocks due to a message passed by Observer::update to the queue,
            // it should be processed.
             db<BasicConsumer>(TRC) << "[Basic Consumer] run() loop received " << received_bytes << " bytes directly (less common path).\n";
            // Assuming the callback in register_interest is the primary way data is handled for P3.
            // If a message makes it here, it means it was put on the receive queue by the communicator.
            // The callback in register_interest is tied to Component::receive -> _data_callback.
            // So, if receive() above gets a message, the _data_callback should have already been invoked.
            // The main utility of the loop is to keep the thread alive and for potential other tasks.
        }

        {
            std::lock_guard<std::mutex> lock(_latest_test_data.mutex);
            if (_latest_test_data.data_valid) {
                 db<BasicConsumer>(INF) << "[Basic Consumer] " << getName() << " latest known value=" 
                                 << _latest_test_data.value << ", counter=" 
                                 << _latest_test_data.counter << " (for debug/display, not CSV IO)\n";
                // Reset data_valid or handle it as needed if it's a one-shot consumption per update
                // _latest_test_data.data_valid = false; 
            }
        }
        
        usleep(TEST_DATA_PERIOD_US / 5); // Sleep to prevent high CPU usage, check more often
    }
    
    db<BasicConsumer>(INF) << "[Basic Consumer] component " << getName() << " exiting main run loop.\n";
}

void BasicConsumer::handle_test_data(const Message& message) {
    db<BasicConsumer>(TRC) << "[Basic Consumer] handle_test_data() called with message type " 
                         << static_cast<int>(message.message_type()) 
                         << " and unit type " << static_cast<int>(message.unit_type()) 
                         << " from " << message.origin().to_string() << "\n";
    
    // Log RESPONSE_RECEIVED event to CSV
    if (_log_file.is_open()) {
        auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        _log_file << now_us << ","                                 // timestamp_us
                 << "CONSUMER" << ","                             // event_category
                 << "RESPONSE_RECEIVED" << ","                   // event_type
                 << message.timestamp() << ","                   // message_id (original msg timestamp)
                 << static_cast<int>(Message::Type::RESPONSE) << "," // message_type (explicitly RESPONSE)
                 << static_cast<int>(message.unit_type()) << ","    // data_type_id
                 << message.origin().to_string() << ","          // origin_address
                 << address().to_string() << ","                  // destination_address (Consumer's own)
                 << "0" << ","                                  // period_us (0 for responses)
                 << message.value_size() << ","                  // value_size
                 << "-"                                         // notes
                 << "\n";
        _log_file.flush();
    }
    
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
