#include <iostream>
#include <memory>
#include <vector>
#include <chrono>
#include <thread>
#include <sstream>

#include "../../include/component.h"
#include "../../include/vehicle.h"
#include "../../include/initializer.h"
#include "../test_utils.h"

// Component that allocates memory to check for leaks
class MemoryTestComponent : public Component {
public:
    MemoryTestComponent(Vehicle* vehicle, const std::string& name, TheProtocol* protocol, TheAddress address)
        : Component(vehicle, name, protocol, address) {
        // Allocate some memory to track
        for (int i = 0; i < 5; i++) {
            data_blocks.push_back(std::make_unique<char[]>(1024 * 1024)); // 1MB blocks
            db<Component>(INF) << "Component " << _name << " allocated 1MB block\n";
        }
    }

    void run() override {
        db<Component>(INF) << "MemoryTestComponent " << _name << " running\n";
        while (_running) {
            // Just wait
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        db<Component>(INF) << "MemoryTestComponent " << _name << " stopped\n";
    }

private:
    // Track some memory allocations in this component
    std::vector<std::unique_ptr<char[]>> data_blocks;
};

// Repeatedly create and destroy vehicles with components
void test_component_memory_management() {
    TEST_INIT("Component Memory Management");

    for (int i = 0; i < 3; i++) {
        std::stringstream ss;
        ss << "Iteration " << i << " - Creating vehicle and components";
        TEST_LOG(ss.str());

        // Create a vehicle
        Vehicle* vehicle = Initializer::create_vehicle(i);
        TEST_ASSERT(vehicle != nullptr, "Vehicle creation failed");

        // Create memory-intensive components
        for (int j = 0; j < 3; j++) {
            MemoryTestComponent* comp = Initializer::create_component<MemoryTestComponent>(
                vehicle, "MemTest" + std::to_string(j));
            TEST_ASSERT(comp != nullptr, "Component creation failed");

            // Start each component
            comp->start();
        }
        
        // Let them run briefly
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Stop and destroy everything
        std::stringstream ss2;
        ss2 << "Stopping and destroying vehicle " << i;
        TEST_LOG(ss2.str());
        vehicle->stop();
        delete vehicle;
        
        std::stringstream ss3;
        ss3 << "Iteration " << i << " completed - Vehicle and components destroyed";
        TEST_LOG(ss3.str());
    }
}

// Test vehicle component ownership and lifecycle management
void test_vehicle_component_ownership() {
    TEST_INIT("Vehicle Component Ownership");

    // Create a vehicle
    Vehicle* vehicle = Initializer::create_vehicle(1);
    TEST_ASSERT(vehicle != nullptr, "Vehicle creation failed");

    // Create several test components
    std::vector<MemoryTestComponent*> components;
    for (int i = 0; i < 5; i++) {
        MemoryTestComponent* comp = Initializer::create_component<MemoryTestComponent>(
            vehicle, "OwnershipTest" + std::to_string(i));
        TEST_ASSERT(comp != nullptr, "Component creation failed");
        components.push_back(comp);
    }
    
    // Start all components
    vehicle->start();
    TEST_LOG("Started all components");
    
    // Let them run briefly
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Stop and destroy the vehicle - this should properly clean up all components
    TEST_LOG("Stopping and destroying vehicle");
    vehicle->stop();
    delete vehicle;
    
    // If we get here without crashes, the test passed
    TEST_LOG("Vehicle and components destroyed properly");
}

int main() {
    std::cout << "--- Starting Component Memory Management Integration Tests ---" << std::endl;

    test_component_memory_management();
    test_vehicle_component_ownership();

    std::cout << "--- Component Memory Management Integration Tests Completed ---" << std::endl;

    std::cout << "For detailed memory leak detection, run with Valgrind:" << std::endl;
    std::cout << "valgrind --leak-check=full --show-leak-kinds=all ./bin/integration_tests/memory_management_test" << std::endl;

    return 0;
} 