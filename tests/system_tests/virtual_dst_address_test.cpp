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
    TEST_INIT("vehicle_to_vehicle_component_test");

    // 1. Create two vehicles
    Vehicle* v1 = Initializer::create_vehicle(101);
    Vehicle* v2 = Initializer::create_vehicle(202);

    // 2. Add components to both vehicles (order determines port assignment)
    ECUComponent* v1_ecu1 = Initializer::create_component<ECUComponent>(v1, "ECU1");
    ECUComponent* v1_ecu2 = Initializer::create_component<ECUComponent>(v1, "ECU2");
    CameraComponent* v1_camera = Initializer::create_component<CameraComponent>(v1, "Camera");

    ECUComponent* v2_ecu1 = Initializer::create_component<ECUComponent>(v2, "ECU1");
    ECUComponent* v2_ecu2 = Initializer::create_component<ECUComponent>(v2, "ECU2");
    CameraComponent* v2_camera = Initializer::create_component<CameraComponent>(v2, "Camera");

    // 3. Start both vehicles (starts all components)
    v1->start();
    v2->start();

    // 4. Compose the destination address for v2's ECU2 (vehicle 2's MAC, port 1)
    TheAddress dest_addr = TheAddress(v2->address(), ECU2_PORT); // Assuming ECU2_PORT is defined in the component header

    // 5. Compose a test message
    std::string test_msg = "[Test] Vehicle 101 to Vehicle 202 ECU2";

    // 6. Send the message from v1's ECU1 to v2's ECU2
    int bytes_sent = v1_ecu1->send(dest_addr, test_msg.c_str(), test_msg.size());
    TEST_ASSERT(bytes_sent == (int)test_msg.size(), "Message should be sent successfully");

    // 7. Wait a short time for message delivery and processing
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 8. Clean up: stop and delete vehicles (which stops and deletes components)
    v1->stop();
    v2->stop();
    delete v1;
    delete v2;

    TEST_LOG("Vehicle-to-vehicle component addressing test completed successfully.");
    return 0;
}