#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>
#include "initializer.h"
#include "ethernet.h"
#include "vehicle.h"
#include "debug.h"
#include "protocol.h"
#include "component.h"
#include "components/ecu_component.h"
#include "components/camera_component.h"
#include "components/lidar_component.h"
#include "components/ins_component.h"
#include "components/battery_component.h"
#include "test_utils.h"

int main() {
    TEST_INIT("Virtual Destination Address Test");

    // 1. Create a vehicle
    Vehicle* v1 = Initializer::create_vehicle(201);

    // 2. Add components to vehicle (order determines port assignment)
    Initializer::create_component<ECUComponent>(v1, "ECU1");
    Initializer::create_component<ECUComponent>(v1, "ECU2");
    LidarComponent* v1_lidar = Initializer::create_component<LidarComponent>(v1, "Lidar");
    Initializer::create_component<INSComponent>(v1, "INS");
    Initializer::create_component<BatteryComponent>(v1, "Battery");

    // 3. Start vehicle (starts all components)
    v1->start();

    // 4. Compose the destination address for v1's ECU1 (vehicle 1's MAC, port 1)
    TheAddress dest_addr = v1->address(); // Assuming ECU1_PORT is defined in the component header
    dest_addr.port(static_cast<unsigned int>(ECU1_PORT)); // Set the port to ECU1's port

    // 5. Compose a test message
    std::string test_msg = "[Test] Lidar to ECU1";

    // 6. Send the message from v1's Lidar to v1's ECU1
    int bytes_sent = v1_lidar->send(dest_addr, test_msg.c_str(), test_msg.size());
    TEST_ASSERT(bytes_sent == (int)test_msg.size(), "Message should be sent successfully");

    // 7. Wait a short time for message delivery and processing
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Next test proves the local broadcast is viable
    // 8. Compose the destination address for local broadcast (vehicle 1's MAC, port 0)
    TheAddress broadcast_addr = v1->address(); // Assuming ECU1_PORT is defined in the component header
    broadcast_addr.port(static_cast<unsigned int>(0)); // Ensure the port is set to port 0

    // 9. Compose a test message
    std::string test_msg = "[Test] Lidar to All";

    byte_sent = v1_lidar->send(broadcast_addr, test_msg.c_str(), test_msg.size());
    TEST_ASSERT(bytes_sent == (int)test_msg.size(), "Broadcast message should be sent successfully");

    // 8. Clean up: delete vehicles (which stops and deletes components)
    delete v1;

    TEST_LOG("Vehicle-to-vehicle component addressing test completed successfully.");
    return 0;
}