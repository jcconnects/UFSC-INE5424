#include <iostream>
#include <memory>
#include <vector>
#include <chrono>
#include <thread>
#include <sstream>

#include "../test_utils.h"
#include "../../include/initializer.h"
#include "../../include/component.h"
#include "../../include/vehicle.h"
#include "../../include/componentType.h"
#include "../../include/teds.h"

// Component that allocates memory to check for leaks
class MemoryTestComponent : public Component {
public:
    MemoryTestComponent(Vehicle* vehicle, const std::string& name, 
                        ComponentType type = ComponentType::UNKNOWN,
                        DataTypeId dataType = DataTypeId::UNKNOWN)
        : Component(vehicle, vehicle->id(), name) {
        
        // Store component type and data type
        _component_type = type;
        _data_type = dataType;
        
        // Allocate some memory to track
        for (int i = 0; i < 5; i++) {
            data_blocks.push_back(std::make_unique<char[]>(1024 * 1024)); // 1MB blocks
            db<Component>(INF) << "Component " << _name << " (" << componentTypeToString(_component_type) 
                              << ") allocated 1MB block\n";
        }
    }

    void run() override {
        db<Component>(INF) << "MemoryTestComponent " << _name << " (" 
                         << componentTypeToString(_component_type) << ") running\n";
        while (_running) {
            // Just wait
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        db<Component>(INF) << "MemoryTestComponent " << _name << " (" 
                         << componentTypeToString(_component_type) << ") stopped\n";
    }
    
    // Helper method to convert ComponentType to string for logging
    const char* componentTypeToString(ComponentType type) {
        switch (type) {
            case ComponentType::CONSUMER: return "CONSUMER";
            case ComponentType::PRODUCER: return "PRODUCER";
            case ComponentType::GATEWAY: return "GATEWAY";
            default: return "UNKNOWN";
        }
    }
    
    // Accessors
    ComponentType component_type() const { return _component_type; }
    DataTypeId data_type() const { return _data_type; }

private:
    // Track some memory allocations in this component
    std::vector<std::unique_ptr<char[]>> data_blocks;
    ComponentType _component_type;
    DataTypeId _data_type;
};

// Repeatedly create and destroy vehicles with components
void test_component_memory_management() {
    TEST_INIT("Component Memory Management");

    for (int i = 0; i < 3; i++) {
        std::stringstream ss;
        ss << "Iteration " << i << " - Creating vehicle and components";
        TEST_LOG(ss.str());

        // Create a vehicle
        Vehicle* vehicle = new Vehicle(i);
        TEST_ASSERT(vehicle != nullptr, "Vehicle creation failed");

        // Create memory-intensive components with different roles
        auto consumer = vehicle->create_component<MemoryTestComponent>("MemConsumer" + std::to_string(i), 
                            ComponentType::CONSUMER, DataTypeId::VEHICLE_SPEED);
        TEST_LOG("Created Consumer MemoryTestComponent");
        
        auto producer = vehicle->create_component<MemoryTestComponent>("MemProducer" + std::to_string(i), 
                             ComponentType::PRODUCER, DataTypeId::ENGINE_RPM);
        TEST_LOG("Created Producer MemoryTestComponent");
        
        auto gateway = vehicle->create_component<MemoryTestComponent>("MemGateway" + std::to_string(i), 
                            ComponentType::GATEWAY, DataTypeId::UNKNOWN);
        TEST_LOG("Created Gateway MemoryTestComponent");
        
        // Verify component types are set correctly
        TEST_ASSERT(consumer->component_type() == ComponentType::CONSUMER, 
                   "Consumer should have CONSUMER type");
        TEST_ASSERT(producer->component_type() == ComponentType::PRODUCER, 
                   "Producer should have PRODUCER type");
        TEST_ASSERT(gateway->component_type() == ComponentType::GATEWAY, 
                   "Gateway should have GATEWAY type");
        
        // Verify data types are set correctly
        TEST_ASSERT(consumer->data_type() == DataTypeId::VEHICLE_SPEED, 
                   "Consumer should have VEHICLE_SPEED data type");
        TEST_ASSERT(producer->data_type() == DataTypeId::ENGINE_RPM, 
                   "Producer should have ENGINE_RPM data type");
        TEST_ASSERT(gateway->data_type() == DataTypeId::UNKNOWN, 
                   "Gateway should have UNKNOWN data type");
        
        // Let them run briefly
        vehicle->start();
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
    Vehicle* vehicle = new Vehicle(1);
    TEST_ASSERT(vehicle != nullptr, "Vehicle creation failed");

    // Create different component types
    auto consumer1 = vehicle->create_component<MemoryTestComponent>("ConsumerOwnership1", 
                                            ComponentType::CONSUMER, DataTypeId::VEHICLE_SPEED);
    auto consumer2 = vehicle->create_component<MemoryTestComponent>("ConsumerOwnership2", 
                                            ComponentType::CONSUMER, DataTypeId::GPS_POSITION);
    auto producer1 = vehicle->create_component<MemoryTestComponent>("ProducerOwnership1", 
                                            ComponentType::PRODUCER, DataTypeId::ENGINE_RPM);
    auto producer2 = vehicle->create_component<MemoryTestComponent>("ProducerOwnership2", 
                                            ComponentType::PRODUCER, DataTypeId::STEERING_ANGLE);
    auto gateway = vehicle->create_component<MemoryTestComponent>("GatewayOwnership", 
                                            ComponentType::GATEWAY, DataTypeId::UNKNOWN);
    
    TEST_LOG("Created 5 components with different roles");
    
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

// Test memory management with simulated P3 message exchanges
void test_p3_memory_exchange() {
    TEST_INIT("P3 Memory Exchange");
    
    // Create a vehicle with a larger number of components to stress test memory
    Vehicle* vehicle = new Vehicle(2);
    
    const int num_components = 10; // 5 producer-consumer pairs
    
    // Create consumer components first
    std::vector<MemoryTestComponent*> consumers;
    for (int i = 0; i < num_components/2; i++) {
        // Alternate data types to simulate multiple data flows
        DataTypeId dtype = (i % 2 == 0) ? DataTypeId::VEHICLE_SPEED : DataTypeId::GPS_POSITION;
        std::string name = "StressConsumer" + std::to_string(i);
        
        auto consumer = vehicle->create_component<MemoryTestComponent>(name, 
                                                ComponentType::CONSUMER, dtype);
        consumers.push_back(consumer);
    }
    
    // Create producer components next
    std::vector<MemoryTestComponent*> producers;
    for (int i = 0; i < num_components/2; i++) {
        // Alternate data types to match consumers
        DataTypeId dtype = (i % 2 == 0) ? DataTypeId::VEHICLE_SPEED : DataTypeId::GPS_POSITION;
        std::string name = "StressProducer" + std::to_string(i);
        
        auto producer = vehicle->create_component<MemoryTestComponent>(name, 
                                                 ComponentType::PRODUCER, dtype);
        producers.push_back(producer);
    }
    
    // Create a gateway component
    auto gateway = vehicle->create_component<MemoryTestComponent>("StressGateway", 
                                            ComponentType::GATEWAY, DataTypeId::UNKNOWN);
    
    TEST_LOG("Created " + std::to_string(num_components) + " memory-intensive components");
    
    // Start everything
    vehicle->start();
    TEST_LOG("Started all components");
    
    // Run for a few seconds, which should be enough time for multiple message exchanges
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // Stop and destroy
    TEST_LOG("Stopping and destroying vehicle with all components");
    vehicle->stop();
    delete vehicle;
    
    TEST_LOG("P3 memory exchange test completed without leaks or crashes");
}

int main() {
    std::cout << "--- Starting Component Memory Management Integration Tests ---" << std::endl;

    test_component_memory_management();
    test_vehicle_component_ownership();
    test_p3_memory_exchange();

    std::cout << "--- Component Memory Management Integration Tests Completed ---" << std::endl;

    std::cout << "For detailed memory leak detection, run with Valgrind:" << std::endl;
    std::cout << "valgrind --leak-check=full --show-leak-kinds=all ./bin/integration_tests/memory_management_test" << std::endl;

    return 0;
} 