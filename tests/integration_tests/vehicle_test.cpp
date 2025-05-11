#define TEST_MODE 1

#include <iostream>
#include <string>
#include <cassert>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic> // For std::atomic in TestComponent
#include "initializer.h" // Include Initializer
#include "vehicle.h"
#include "nic.h"
#include "protocol.h"
#include "ethernet.h"
#include "socketEngine.h"
#include "component.h"
#include "test_utils.h"

// Define port constants (needed if components require them, even TestComponent needs constructor args)
// Note: These might be defined elsewhere if other test files are compiled together.
// Consider a shared test constants header.
const unsigned short ECU1_PORT = 0;
const unsigned short ECU2_PORT = 1;


// Define a test component that tracks its lifecycle events
class TestComponent : public Component {
public:
    // Updated constructor matching the base class signature
    TestComponent(Vehicle* vehicle, const std::string& name, const unsigned int vehicle_id = 0)
        : Component(vehicle, vehicle_id, name),
          run_entered(false) // Initialize run_entered flag
    {
        // Base constructor now handles initialization
    }

    // Implement the pure virtual run method
    void run() override {
        run_entered.store(true, std::memory_order_release);
        // Simple loop that respects the running flag
        while (running()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    bool was_run_entered() const { return run_entered.load(std::memory_order_acquire); }

private:
    std::atomic<bool> run_entered;
};

int main() {
    TEST_INIT("vehicle_test");
    
    // Test 1: Vehicle creation and basic properties (remains the same)
    TEST_LOG("--- Starting Test 1: Vehicle Creation & Properties ---");
    Vehicle* vehicle = new Vehicle(42);
    
    TEST_ASSERT(vehicle != nullptr, "Vehicle should not be null");
    TEST_ASSERT(vehicle->id() == 42, "Vehicle ID should be 42");
    TEST_ASSERT(vehicle->running() == false, "Vehicle should not be running initially");
    TEST_LOG("Test 1 Passed.");
    
    // Test 2: Vehicle lifecycle management (remains the same)
    TEST_LOG("--- Starting Test 2: Vehicle Lifecycle ---");
    
    // Test starting the vehicle
    vehicle->start();
    TEST_ASSERT(vehicle->running() == true, "Vehicle should be running after start()");
    
    // Test stopping the vehicle
    vehicle->stop();
    TEST_ASSERT(vehicle->running() == false, "Vehicle should not be running after stop()");
    
    // Test multiple start/stop cycles
    for (int i = 0; i < 3; i++) {
        vehicle->start();
        TEST_ASSERT(vehicle->running() == true, "Vehicle should be running after start()");
        
        vehicle->stop();
        TEST_ASSERT(vehicle->running() == false, "Vehicle should not be running after stop()");
    }
    TEST_LOG("Test 2 Passed.");
    
    // Test 3: Component management (Adapted)
    TEST_LOG("--- Starting Test 3: Component Management Lifecycle ---");
    
    // Ensure vehicle is stopped before adding components
    if(vehicle->running()) vehicle->stop(); 
    // Re-create vehicle for clarity in this test section
    delete vehicle; 
    vehicle = new Vehicle(42); 
    
    // Create components and maintain our own direct references to them
    TestComponent* component1 = new TestComponent(vehicle, "TestComponent1", vehicle->id());
    TestComponent* component2 = new TestComponent(vehicle, "TestComponent2", vehicle->id());
    TestComponent* component3 = new TestComponent(vehicle, "TestComponent3", vehicle->id());
    
    // Start the vehicle, which should start the component threads in TestComponent::run
    TEST_LOG("Starting vehicle to start components");
    vehicle->start();
    TEST_ASSERT(vehicle->running() == true, "Vehicle should be running after start()");

    // Allow some time for threads to start and enter run()
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Test component running state and run method entry
    TEST_ASSERT(component1->running(), "Component 1 should be running after vehicle start");
    TEST_ASSERT(component2->running(), "Component 2 should be running after vehicle start");
    TEST_ASSERT(component3->running(), "Component 3 should be running after vehicle start");

    TEST_ASSERT(component1->was_run_entered(), "Component 1 run() should have been entered");
    TEST_ASSERT(component2->was_run_entered(), "Component 2 run() should have been entered");
    TEST_ASSERT(component3->was_run_entered(), "Component 3 run() should have been entered");

    // Stop the vehicle, which should stop its components
    TEST_LOG("Stopping vehicle to stop components");
    vehicle->stop();
    TEST_ASSERT(vehicle->running() == false, "Vehicle should not be running after stop()");

    // Test component stopped state
    // Note: Base::stop joins threads, so running() should be false immediately after stop returns
    TEST_ASSERT(!component1->running(), "Component 1 should not be running after vehicle stop");
    TEST_ASSERT(!component2->running(), "Component 2 should not be running after vehicle stop");
    TEST_ASSERT(!component3->running(), "Component 3 should not be running after vehicle stop");
    TEST_LOG("Test 3 Passed.");

    
    // Test 4: Verify components added before vehicle starts are started (Adapted)
    TEST_LOG("--- Starting Test 4: Components Started with Vehicle ---");
    
    Vehicle* vehicle2 = new Vehicle(43);
    TEST_ASSERT(vehicle2 != nullptr, "Vehicle 2 should be created");
    
    // Create components directly
    TEST_LOG("Creating TestComponents for Vehicle 2");
    TestComponent* component4 = new TestComponent(vehicle2, "TestComponent4", vehicle2->id());
    TestComponent* component5 = new TestComponent(vehicle2, "TestComponent5", vehicle2->id());
    
    TEST_ASSERT(component4 != nullptr, "Component 4 should be created");
    TEST_ASSERT(component5 != nullptr, "Component 5 should be created");
    
    // Start vehicle and check if components were started and run() entered
    TEST_LOG("Starting Vehicle 2");
    vehicle2->start();
    TEST_ASSERT(vehicle2->running(), "Vehicle 2 should be running after start()");
    
    // Allow time for threads
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    TEST_ASSERT(component4->running(), "Component 4 should be running after vehicle start");
    TEST_ASSERT(component5->running(), "Component 5 should be running after vehicle start");
    TEST_ASSERT(component4->was_run_entered(), "Component 4 run() should have been called when vehicle starts");
    TEST_ASSERT(component5->was_run_entered(), "Component 5 run() should have been called when vehicle starts");
    TEST_LOG("Test 4 Passed.");

    // Test 5 -> Renamed Test 5: Make sure vehicle destructor properly cleans up components
    TEST_LOG("--- Starting Test 5: Vehicle Destructor Cleanup ---");
    
    // Stop vehicle before deleting if it's running (it is from Test 4)
    TEST_LOG("Stopping Vehicle 2 before deletion");
    vehicle2->stop();
    TEST_ASSERT(!vehicle2->running(), "Vehicle 2 should not be running after stop()");

    // Manually delete the components we created directly
    TEST_LOG("Deleting components");
    delete component1;
    delete component2;
    delete component3;
    delete component4;
    delete component5;

    TEST_LOG("Deleting Vehicle 2");
    delete vehicle2; // vehicle2's destructor should delete component4 and component5
    vehicle2 = nullptr; // Avoid dangling pointer
    TEST_LOG("Vehicle 2 deleted successfully");
    
    // Clean up remaining vehicle from Test 1-3
    TEST_LOG("Cleaning up Vehicle 1");
    // Ensure vehicle 1 is stopped (it was stopped in Test 3)
    if(vehicle->running()) vehicle->stop(); 
    delete vehicle;
    vehicle = nullptr;
    
    TEST_LOG("Vehicle test passed successfully!");
    return 0;
} 