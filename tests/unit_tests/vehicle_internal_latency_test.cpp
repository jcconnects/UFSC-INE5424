#include <iostream>
#include <unistd.h>
#include <chrono>
#include <thread>

#include "app/vehicle.h"
#include "api/util/debug.h"
#include "api/framework/location_service.h"
#include "app/datatypes.h"
#include "../testcase.h"

/**
 * @brief Test internal communication latency between vehicle components
 * 
 * This test creates vehicle 10081 with multiple components that interact
 * with each other to generate realistic internal communication latency
 * data that will be captured in CSV logs.
 */
class VehicleInternalLatencyTest : public TestCase {
public:
    VehicleInternalLatencyTest();
    ~VehicleInternalLatencyTest() = default;

    void setUp() override;
    void tearDown() override;

    void testComprehensiveComponentInteraction();

private:
    void setup_comprehensive_components(Vehicle* vehicle);
    void run_latency_test(Vehicle* vehicle, unsigned int duration_seconds);
};

VehicleInternalLatencyTest::VehicleInternalLatencyTest() {
    DEFINE_TEST(testComprehensiveComponentInteraction);
}

void VehicleInternalLatencyTest::setUp() {
    // Set up test environment
    LocationService::setCurrentCoordinates(1000.0, 1000.0);
    db<VehicleInternalLatencyTest>(INF) << "Vehicle internal latency test setup completed\n";
}

void VehicleInternalLatencyTest::tearDown() {
    db<VehicleInternalLatencyTest>(INF) << "Vehicle internal latency test teardown completed\n";
}

/**
 * @brief Test comprehensive component interaction with vehicle ID 10081
 * 
 * Creates multiple components and establishes various communication patterns
 * to generate realistic latency measurements captured in CSV logs.
 */
void VehicleInternalLatencyTest::testComprehensiveComponentInteraction() {
    std::cout << "Starting comprehensive component interaction test with vehicle 10081" << std::endl;
    
    // Create vehicle with ID 10081 as requested
    Vehicle* vehicle = new Vehicle(10081);
    
    // Set transmission radius for internal testing
    vehicle->setTransmissionRadius(1000.0);
    
    // Set up comprehensive component configuration
    setup_comprehensive_components(vehicle);
    
    // Verify components were created
    // assert_true(vehicle->component_count() >= 10, "Vehicle should have multiple components for latency testing");
    
    std::cout << "Vehicle 10081 created with " << vehicle->component_count() << " components" << std::endl;
    
    // Start vehicle and run latency test
    vehicle->start();
    std::cout << "Vehicle 10081 started, beginning latency test..." << std::endl;
    
    // Run for sufficient time to capture meaningful latency data
    run_latency_test(vehicle, 20);
    
    // Stop vehicle
    vehicle->stop();
    std::cout << "Vehicle 10081 stopped, latency test completed" << std::endl;
    
    delete vehicle;
    
    std::cout << "Comprehensive component interaction test completed successfully" << std::endl;
    std::cout << "Check CSV logs in tests/logs/vehicle_10081/ for latency data" << std::endl;
}

/**
 * @brief Set up comprehensive component configuration for latency testing
 * 
 * Creates multiple producer and consumer components with various data types
 * and communication patterns to generate realistic latency measurements.
 * 
 * @param vehicle Pointer to the vehicle instance
 */
void VehicleInternalLatencyTest::setup_comprehensive_components(Vehicle* vehicle) {
    db<VehicleInternalLatencyTest>(INF) << "Setting up comprehensive component configuration for vehicle 10081\n";
    
    // === Basic Producer-Consumer Pairs ===
    vehicle->create_component<BasicProducerA>("ProducerA_Primary");
    vehicle->create_component<BasicProducerA>("ProducerA_Secondary");
    vehicle->create_component<BasicProducerB>("ProducerB_Primary");
    vehicle->create_component<BasicProducerB>("ProducerB_Secondary");
    
    vehicle->create_component<BasicConsumerA>("ConsumerA_High");
    vehicle->create_component<BasicConsumerA>("ConsumerA_Medium");
    vehicle->create_component<BasicConsumerA>("ConsumerA_Low");
    vehicle->create_component<BasicConsumerB>("ConsumerB_High");
    vehicle->create_component<BasicConsumerB>("ConsumerB_Medium");
    vehicle->create_component<BasicConsumerB>("ConsumerB_Low");
    
    // === Automotive Sensor Components ===
    // vehicle->create_component<CameraComponent>("FrontCamera");
    // vehicle->create_component<CameraComponent>("RearCamera");
    // vehicle->create_component<LidarComponent>("MainLidar");
    // vehicle->create_component<INSComponent>("NavigationINS");
    // vehicle->create_component<ECUComponent>("CentralECU");
    // vehicle->create_component<ECUComponent>("SafetyECU");
    
    db<VehicleInternalLatencyTest>(INF) << "Created " << vehicle->component_count() << " components\n";
    
    // === Configure Consumer Periodic Interests ===
    
    // High-frequency consumers (100-200ms periods)
    auto consumerA_High = vehicle->get_component<Agent>("ConsumerA_High");
    auto consumerB_High = vehicle->get_component<Agent>("ConsumerB_High");
    
    if (consumerA_High) {
        consumerA_High->start_periodic_interest(
            static_cast<std::uint32_t>(DataTypes::UNIT_A), 
            Agent::Microseconds(150000)  // 150ms
        );
        db<VehicleInternalLatencyTest>(INF) << "ConsumerA_High configured for 150ms period\n";
    }
    
    if (consumerB_High) {
        consumerB_High->start_periodic_interest(
            static_cast<std::uint32_t>(DataTypes::UNIT_B), 
            Agent::Microseconds(100000)  // 100ms
        );
        db<VehicleInternalLatencyTest>(INF) << "ConsumerB_High configured for 100ms period\n";
    }
    
    // Medium-frequency consumers (300-500ms periods)
    auto consumerA_Medium = vehicle->get_component<Agent>("ConsumerA_Medium");
    auto consumerB_Medium = vehicle->get_component<Agent>("ConsumerB_Medium");
    
    if (consumerA_Medium) {
        consumerA_Medium->start_periodic_interest(
            static_cast<std::uint32_t>(DataTypes::UNIT_A), 
            Agent::Microseconds(350000)  // 350ms
        );
        db<VehicleInternalLatencyTest>(INF) << "ConsumerA_Medium configured for 350ms period\n";
    }
    
    if (consumerB_Medium) {
        consumerB_Medium->start_periodic_interest(
            static_cast<std::uint32_t>(DataTypes::UNIT_B), 
            Agent::Microseconds(450000)  // 450ms
        );
        db<VehicleInternalLatencyTest>(INF) << "ConsumerB_Medium configured for 450ms period\n";
    }
    
    // Low-frequency consumers (700ms+ periods)
    auto consumerA_Low = vehicle->get_component<Agent>("ConsumerA_Low");
    auto consumerB_Low = vehicle->get_component<Agent>("ConsumerB_Low");
    
    if (consumerA_Low) {
        consumerA_Low->start_periodic_interest(
            static_cast<std::uint32_t>(DataTypes::UNIT_A), 
            Agent::Microseconds(800000)  // 800ms
        );
        db<VehicleInternalLatencyTest>(INF) << "ConsumerA_Low configured for 800ms period\n";
    }
    
    if (consumerB_Low) {
        consumerB_Low->start_periodic_interest(
            static_cast<std::uint32_t>(DataTypes::UNIT_B), 
            Agent::Microseconds(1000000)  // 1000ms
        );
        db<VehicleInternalLatencyTest>(INF) << "ConsumerB_Low configured for 1000ms period\n";
    }
    
    // === Configure ECU Consumers for Sensor Data ===
    auto centralECU = vehicle->get_component<Agent>("CentralECU");
    auto safetyECU = vehicle->get_component<Agent>("SafetyECU");
    
    if (centralECU) {
        // Central ECU requests lidar point cloud data at typical automotive rates
        centralECU->start_periodic_interest(
            static_cast<std::uint32_t>(DataTypes::EXTERNAL_POINT_CLOUD_XYZ), 
            Agent::Microseconds(100000)  // 100ms - typical for real-time processing
        );
        db<VehicleInternalLatencyTest>(INF) << "CentralECU configured for lidar data at 100ms period\n";
    }
    
    if (safetyECU) {
        // Safety ECU also requests lidar data but at different frequency
        safetyECU->start_periodic_interest(
            static_cast<std::uint32_t>(DataTypes::EXTERNAL_POINT_CLOUD_XYZ), 
            Agent::Microseconds(200000)  // 200ms - safety checks
        );
        db<VehicleInternalLatencyTest>(INF) << "SafetyECU configured for lidar data at 200ms period\n";
    }
    
    db<VehicleInternalLatencyTest>(INF) << "Comprehensive component configuration completed\n";
}

/**
 * @brief Run latency test for specified duration with progress reporting
 * 
 * Runs the vehicle for the specified duration while providing progress updates
 * to capture comprehensive latency data in CSV logs.
 * 
 * @param vehicle Pointer to the vehicle instance
 * @param duration_seconds Duration to run the test in seconds
 */
void VehicleInternalLatencyTest::run_latency_test(Vehicle* vehicle, unsigned int duration_seconds) {
    db<VehicleInternalLatencyTest>(INF) << "Running latency test for " << duration_seconds 
                                       << " seconds on vehicle 10081\n";
    
    std::cout << "Latency test in progress..." << std::endl;
    
    for (unsigned int i = 0; i < duration_seconds; ++i) {
        sleep(1);
        
        // Progress reporting every 5 seconds
        if ((i + 1) % 5 == 0) {
            std::cout << "Test progress: " << (i + 1) << "/" << duration_seconds 
                     << " seconds - capturing latency data..." << std::endl;
            db<VehicleInternalLatencyTest>(INF) << "Latency test progress: " << (i + 1) 
                                               << "/" << duration_seconds << " seconds\n";
        }
        
        // Log component activity every 10 seconds for debugging
        if ((i + 1) % 10 == 0) {
            db<VehicleInternalLatencyTest>(INF) << "Vehicle 10081 has " << vehicle->component_count() 
                                               << " active components generating communication data\n";
        }
    }
    
    std::cout << "Latency test completed - " << duration_seconds << " seconds of data captured" << std::endl;
    db<VehicleInternalLatencyTest>(INF) << "Latency test completed successfully\n";
}

int main(int argc, char* argv[]) {
    std::cout << "=== Vehicle Internal Communication Latency Test ===" << std::endl;
    std::cout << "Testing vehicle ID: 10081" << std::endl;
    std::cout << "Purpose: Generate representative internal communication latency data" << std::endl;
    std::cout << "========================================================" << std::endl;
    
    VehicleInternalLatencyTest test;
    test.run();
    
    std::cout << "========================================================" << std::endl;
    std::cout << "Test completed. Check CSV logs for latency measurements." << std::endl;
    
    return 0;
} 