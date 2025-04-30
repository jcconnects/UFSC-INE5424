#include "test_utils.h"
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

int main() {
    TEST_INIT("Broadcast Test");

    // 1. Create a group of vehicles
    TEST_LOG("Creating 5 vehicles with IDs 200 to 204.");
    Vehicle** vehicles = new Vehicle*[5];
    for (int i = 0; i < 5; ++i) {
        vehicles[i] = Initializer::create_vehicle(200 + i);
    }

    // 2. Create components for each vehicle
    for (int i = 0; i < 5; ++i) {
        Initializer::create_component<ECUComponent>(vehicles[i], "ECU");
        Initializer::create_component<LidarComponent>(vehicles[i], "Lidar");
        Initializer::create_component<INSComponent>(vehicles[i], "INS");
        Initializer::create_component<BatteryComponent>(vehicles[i], "Battery");
    }

    // 3. Start all vehicles (which starts all components)
    for (int i = 0; i < 5; ++i) {
        vehicles[i]->start();
    }

    // 4. Compose a test message
    std::string test_msg = "[BROADCAST] Vehicle 201 to all ECUs";

    // 5. Create sending component
    LidarComponent* sender = Initializer::create_component<LidarComponent>(vehicles[0], "Sender");
    TheAddress broadcast_address = Protocol<NIC<SocketEngine, SharedMemoryEngine>>::Address::BROADCAST;
    broadcast_address.port(ECU1_PORT); // Set the port to 0 for broadcast
    sender->send(broadcast_address, test_msg.c_str(), test_msg.size());

    // 6. Sleep for a while to allow all components to process the broadcast message
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 7. Clean up: stop and delete vehicles (which stops and deletes components)
    for (int i = 0; i < 5; ++i) {
        delete vehicles[i];
    }
    TEST_LOG("Vehicles deleted successfully.");
    delete [] vehicles;
    TEST_LOG("Vehicle array deleted successfully.");

    TEST_LOG("Vehicle-to-vehicle component addressing test completed successfully.");
    return 0;
}