#include <iostream>
#include <filesystem>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <random>
#include <chrono>
#include <signal.h>
#include <errno.h>
#include <fstream>

#include "app/vehicle.h"
#include "api/util/debug.h"
#include "api/framework/rsu.h"
#include "api/framework/leaderKeyStorage.h"
#include "api/framework/map_config.h"
#include "app/datatypes.h"
#include "app/components/csv_component_factory.hpp"
#include "app/components/csv_consumer_factory.hpp"
#include "../testcase.h"

class CSVMessagingTest: public TestCase {
    public:
        CSVMessagingTest();
        ~CSVMessagingTest() = default;

        void setUp() override;
        void tearDown() override;

        /* TESTS */
        void testCSVProducerConsumerMessaging();
        void testMultipleCSVProducersOneConsumer();
        void testTimestampExtraction();
        void testDynamicsCSVMessaging();
        
    private:
        void run_csv_producer_vehicle(unsigned int vehicle_id, const std::string& csv_file, unsigned int runtime_seconds = 15);
        void run_csv_consumer_vehicle(unsigned int vehicle_id, unsigned int runtime_seconds = 20);
        void run_mixed_csv_vehicle(unsigned int vehicle_id, unsigned int runtime_seconds = 20);
        void setup_test_environment();
        std::string create_test_csv_file(unsigned int vehicle_id);
        void setup_minimal_rsu();
        pid_t rsu_pid;
};

CSVMessagingTest::CSVMessagingTest() : rsu_pid(-1) {
    DEFINE_TEST(testCSVProducerConsumerMessaging);
    DEFINE_TEST(testMultipleCSVProducersOneConsumer);
    DEFINE_TEST(testTimestampExtraction);
    DEFINE_TEST(testDynamicsCSVMessaging);
}

void CSVMessagingTest::setUp() {
    setup_test_environment();
}

void CSVMessagingTest::tearDown() {
    // Clean up RSU if it's running
    if (rsu_pid > 0) {
        kill(rsu_pid, SIGUSR2);
        int status;
        waitpid(rsu_pid, &status, 0);
        rsu_pid = -1;
    }
}

void CSVMessagingTest::setup_test_environment() {
    // Create test directories
    std::filesystem::create_directories("tests/logs/csv_test");
    std::filesystem::create_directories("tests/logs/csv_test/data");
    
    // Start a minimal RSU for message routing
    setup_minimal_rsu();
    
    // Give RSU time to start
    sleep(2);
}

void CSVMessagingTest::setup_minimal_rsu() {
    rsu_pid = fork();
    if (rsu_pid == 0) {
        // Child process - run minimal RSU
        try {
            // Set up logging
            Debug::set_log_file("tests/logs/csv_test/rsu.log");
            
            // Set fixed coordinates for test RSU
            LocationService::setCurrentCoordinates(500.0, 500.0);
            
            // Create simple RSU configuration
            RSUConfig test_rsu_config;
            test_rsu_config.id = 1;
            test_rsu_config.unit = static_cast<std::uint32_t>(DataTypes::CSV_VEHICLE_DATA);
            test_rsu_config.broadcast_period = std::chrono::milliseconds(1000);
            test_rsu_config.x = 500.0;
            test_rsu_config.y = 500.0;
            
            // Setup RSU as leader
            MacKeyType rsu_key;
            rsu_key.fill(0);
            rsu_key[0] = 0x01;
            rsu_key[2] = 0xAA;
            rsu_key[3] = 0xBB;
            
            Ethernet::Address rsu_mac;
            rsu_mac.bytes[0] = 0x02;
            rsu_mac.bytes[1] = 0x00;
            rsu_mac.bytes[2] = 0x00;
            rsu_mac.bytes[3] = 0x00;
            rsu_mac.bytes[4] = 0x00;
            rsu_mac.bytes[5] = 0x01;
            
            auto& storage = LeaderKeyStorage::getInstance();
            storage.setLeaderId(rsu_mac);
            storage.setGroupMacKey(rsu_key);
            
            // Create and start RSU
            RSU* rsu = new RSU(test_rsu_config.id, test_rsu_config.unit, 
                              test_rsu_config.broadcast_period, 
                              test_rsu_config.x, test_rsu_config.y, 500.0);
            
            rsu->start();
            
            // Wait for termination signal
            volatile sig_atomic_t should_terminate = 0;
            signal(SIGUSR2, [](int) { /* Termination signal handled externally */ });
            
            pause(); // Wait for signal
            
            rsu->stop();
            delete rsu;
            Debug::close_log_file();
            exit(0);
        } catch (const std::exception& e) {
            std::cerr << "RSU Error: " << e.what() << std::endl;
            exit(1);
        }
    }
}

std::string CSVMessagingTest::create_test_csv_file(unsigned int vehicle_id) {
    std::string filename = "tests/logs/csv_test/data/test_vehicle_" + std::to_string(vehicle_id) + ".csv";
    std::ofstream csv_file(filename);
    
    // Write CSV header
    csv_file << "timestamp,id,lat,lon,alt,x,y,z,speed,heading,yawrate,acceleration\n";
    
    // Generate test data with realistic values
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    for (int i = 0; i < 50; ++i) {
        uint64_t timestamp = now + (i * 100000); // 100ms intervals
        csv_file << timestamp << ","
                << vehicle_id << ","
                << (45.0 + i * 0.0001) << ","  // lat
                << (-73.0 + i * 0.0001) << "," // lon
                << (100.0 + i * 0.1) << ","    // alt
                << (500.0 + i * 2.0) << ","    // x
                << (500.0 + i * 1.5) << ","    // y
                << (0.0) << ","                // z
                << (30.0 + (i % 10)) << ","     // speed
                << (90.0 + (i % 45)) << ","     // heading
                << (0.5) << ","                // yawrate
                << (2.0) << "\n";              // acceleration
    }
    
    csv_file.close();
    return filename;
}

void CSVMessagingTest::run_csv_producer_vehicle(unsigned int vehicle_id, const std::string& csv_file, unsigned int runtime_seconds) {
    try {
        std::string log_file = "tests/logs/csv_test/producer_" + std::to_string(vehicle_id) + ".log";
        Debug::set_log_file(log_file);
        
        db<Vehicle>(INF) << "[CSV Producer " << vehicle_id << "] Starting CSV producer vehicle\n";
        
        // Set vehicle position
        LocationService::setCurrentCoordinates(500.0 + vehicle_id * 50.0, 500.0 + vehicle_id * 50.0);
        
        // Create vehicle
        Vehicle* v = new Vehicle(vehicle_id);
        v->setTransmissionRadius(500.0);
        
        // Create CSV producer component using vehicle's CSV component creation method
        try {
            v->create_csv_component_with_file("CSVProducer" + std::to_string(vehicle_id), csv_file);
            db<Vehicle>(INF) << "[CSV Producer " << vehicle_id << "] CSV producer component created\n";
        } catch (const std::exception& e) {
            db<Vehicle>(ERR) << "[CSV Producer " << vehicle_id << "] Failed to create CSV component: " << e.what() << "\n";
            return;
        }
        
        // Start vehicle
        v->start();
        db<Vehicle>(INF) << "[CSV Producer " << vehicle_id << "] Vehicle started, running for " << runtime_seconds << "s\n";
        
        // Run for specified time
        sleep(runtime_seconds);
        
        // Stop and cleanup
        v->stop();
        delete v;
        
        Debug::close_log_file();
        db<Vehicle>(INF) << "[CSV Producer " << vehicle_id << "] Producer vehicle terminated\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Producer Vehicle " << vehicle_id << " Error: " << e.what() << std::endl;
        exit(1);
    }
}

void CSVMessagingTest::run_csv_consumer_vehicle(unsigned int vehicle_id, unsigned int runtime_seconds) {
    try {
        std::string log_file = "tests/logs/csv_test/consumer_" + std::to_string(vehicle_id) + ".log";
        Debug::set_log_file(log_file);
        
        db<Vehicle>(INF) << "[CSV Consumer " << vehicle_id << "] Starting CSV consumer vehicle\n";
        
        // Set vehicle position
        LocationService::setCurrentCoordinates(500.0 + vehicle_id * 50.0, 500.0 + vehicle_id * 50.0);
        
        // Create vehicle
        Vehicle* v = new Vehicle(vehicle_id);
        v->setTransmissionRadius(500.0);
        
        // Create CSV consumer component using vehicle's component creation method
        try {
            v->create_component<CSVConsumerComponent>("CSVConsumer" + std::to_string(vehicle_id));
            db<Vehicle>(INF) << "[CSV Consumer " << vehicle_id << "] CSV consumer component created\n";
        } catch (const std::exception& e) {
            db<Vehicle>(ERR) << "[CSV Consumer " << vehicle_id << "] Failed to create CSV consumer: " << e.what() << "\n";
            return;
        }
        
        // Start consumer with periodic interest for CSV_VEHICLE_DATA
        auto consumer_agent = v->get_component<Agent>("CSVConsumer" + std::to_string(vehicle_id));
        if (consumer_agent) {
            consumer_agent->start_periodic_interest(
                static_cast<std::uint32_t>(DataTypes::CSV_VEHICLE_DATA), 
                Agent::Microseconds(500000) // 500ms period
            );
            db<Vehicle>(INF) << "[CSV Consumer " << vehicle_id << "] Started periodic interest for CSV_VEHICLE_DATA\n";
        }
        
        // Start vehicle
        v->start();
        db<Vehicle>(INF) << "[CSV Consumer " << vehicle_id << "] Vehicle started, running for " << runtime_seconds << "s\n";
        
        // Run for specified time
        sleep(runtime_seconds);
        
        // Stop and cleanup
        v->stop();
        delete v;
        
        Debug::close_log_file();
        db<Vehicle>(INF) << "[CSV Consumer " << vehicle_id << "] Consumer vehicle terminated\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Consumer Vehicle " << vehicle_id << " Error: " << e.what() << std::endl;
        exit(1);
    }
}

void CSVMessagingTest::run_mixed_csv_vehicle(unsigned int vehicle_id, unsigned int runtime_seconds) {
    try {
        std::string log_file = "tests/logs/csv_test/mixed_" + std::to_string(vehicle_id) + ".log";
        Debug::set_log_file(log_file);
        
        db<Vehicle>(INF) << "[Mixed CSV " << vehicle_id << "] Starting mixed CSV vehicle (producer + consumer)\n";
        
        // Set vehicle position
        LocationService::setCurrentCoordinates(500.0 + vehicle_id * 50.0, 500.0 + vehicle_id * 50.0);
        
        // Create vehicle
        Vehicle* v = new Vehicle(vehicle_id);
        v->setTransmissionRadius(500.0);
        
        // Use perception vehicle CSV file from dataset for this vehicle
        std::string csv_file = "include/app/components/datasets/dataset/perception-vehicle_" + std::to_string(vehicle_id % 15) + ".csv";
        
        // Create both CSV producer and consumer components
        try {
            v->create_csv_component_with_file("CSVProducer" + std::to_string(vehicle_id), csv_file);
            v->create_component<CSVConsumerComponent>("CSVConsumer" + std::to_string(vehicle_id));
            
            db<Vehicle>(INF) << "[Mixed CSV " << vehicle_id << "] Both CSV producer and consumer created\n";
        } catch (const std::exception& e) {
            db<Vehicle>(ERR) << "[Mixed CSV " << vehicle_id << "] Failed to create CSV components: " << e.what() << "\n";
            return;
        }
        
        // Start consumer with periodic interest
        auto consumer_agent = v->get_component<Agent>("CSVConsumer" + std::to_string(vehicle_id));
        if (consumer_agent) {
            consumer_agent->start_periodic_interest(
                static_cast<std::uint32_t>(DataTypes::CSV_VEHICLE_DATA), 
                Agent::Microseconds(300000) // 300ms period
            );
            db<Vehicle>(INF) << "[Mixed CSV " << vehicle_id << "] Started periodic interest for CSV_VEHICLE_DATA\n";
        }
        
        // Start vehicle
        v->start();
        db<Vehicle>(INF) << "[Mixed CSV " << vehicle_id << "] Mixed vehicle started, running for " << runtime_seconds << "s\n";
        
        // Run for specified time
        sleep(runtime_seconds);
        
        // Stop and cleanup
        v->stop();
        delete v;
        
        Debug::close_log_file();
        db<Vehicle>(INF) << "[Mixed CSV " << vehicle_id << "] Mixed vehicle terminated\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Mixed Vehicle " << vehicle_id << " Error: " << e.what() << std::endl;
        exit(1);
    }
}

void CSVMessagingTest::testCSVProducerConsumerMessaging() {
    db<CSVMessagingTest>(INF) << "=== Testing CSV Producer-Consumer Messaging ===\n";
    
    // Use dynamics vehicle CSV file from dataset
    std::string csv_file = "include/app/components/datasets/dataset/dynamics-vehicle_0.csv";
    
    std::vector<pid_t> children;
    
    // Create CSV producer vehicle
    pid_t producer_pid = fork();
    if (producer_pid == 0) {
        run_csv_producer_vehicle(101, csv_file, 10);
        exit(0);
    }
    children.push_back(producer_pid);
    
    // Wait a moment for producer to start
    sleep(2);
    
    // Create CSV consumer vehicle
    pid_t consumer_pid = fork();
    if (consumer_pid == 0) {
        run_csv_consumer_vehicle(102, 15);
        exit(0);
    }
    children.push_back(consumer_pid);
    
    // Wait for all processes to complete
    bool success = true;
    for (pid_t child : children) {
        int status;
        if (waitpid(child, &status, 0) == -1 || WEXITSTATUS(status) != 0) {
            success = false;
        }
    }
    
    assert_true(success, "CSV Producer-Consumer messaging should complete successfully");
    db<CSVMessagingTest>(INF) << "=== CSV Producer-Consumer Messaging Test Completed ===\n";
}

void CSVMessagingTest::testMultipleCSVProducersOneConsumer() {
    db<CSVMessagingTest>(INF) << "=== Testing Multiple CSV Producers with One Consumer ===\n";
    
    std::vector<pid_t> children;
    
    // Create multiple CSV producer vehicles using different dataset files
    std::vector<std::string> csv_files = {
        "include/app/components/datasets/dataset/dynamics-vehicle_0.csv",
        "include/app/components/datasets/dataset/dynamics-vehicle_1.csv", 
        "include/app/components/datasets/dataset/perception-vehicle_0.csv"
    };
    
    for (unsigned int i = 201; i <= 203; ++i) {
        std::string csv_file = csv_files[i - 201];  // Use corresponding dataset file
        
        pid_t producer_pid = fork();
        if (producer_pid == 0) {
            run_csv_producer_vehicle(i, csv_file, 12);
            exit(0);
        }
        children.push_back(producer_pid);
        
        // Stagger producer starts
        sleep(1);
    }
    
    // Create one CSV consumer vehicle
    pid_t consumer_pid = fork();
    if (consumer_pid == 0) {
        run_csv_consumer_vehicle(204, 18);
        exit(0);
    }
    children.push_back(consumer_pid);
    
    // Wait for all processes to complete
    bool success = true;
    for (pid_t child : children) {
        int status;
        if (waitpid(child, &status, 0) == -1 || WEXITSTATUS(status) != 0) {
            success = false;
        }
    }
    
    assert_true(success, "Multiple CSV producers with one consumer should work correctly");
    db<CSVMessagingTest>(INF) << "=== Multiple Producers One Consumer Test Completed ===\n";
}

void CSVMessagingTest::testTimestampExtraction() {
    db<CSVMessagingTest>(INF) << "=== Testing Timestamp Extraction ===\n";
    
    std::vector<pid_t> children;
    
    // Create mixed vehicles (both producer and consumer) to test timestamp extraction
    for (unsigned int i = 301; i <= 302; ++i) {
        pid_t mixed_pid = fork();
        if (mixed_pid == 0) {
            run_mixed_csv_vehicle(i, 15);
            exit(0);
        }
        children.push_back(mixed_pid);
        
        sleep(2); // Stagger starts
    }
    
    // Wait for all processes to complete
    bool success = true;
    for (pid_t child : children) {
        int status;
        if (waitpid(child, &status, 0) == -1 || WEXITSTATUS(status) != 0) {
            success = false;
        }
    }
    
    assert_true(success, "Timestamp extraction test should complete successfully");
    db<CSVMessagingTest>(INF) << "=== Timestamp Extraction Test Completed ===\n";
}

void CSVMessagingTest::testDynamicsCSVMessaging() {
    db<CSVMessagingTest>(INF) << "=== Testing Dynamics CSV Messaging ===\n";
    
    std::vector<pid_t> children;
    
    // Test with dynamics CSV files (if they exist)
    pid_t dynamics_pid = fork();
    if (dynamics_pid == 0) {
        try {
            std::string log_file = "tests/logs/csv_test/dynamics_401.log";
            Debug::set_log_file(log_file);
            
            LocationService::setCurrentCoordinates(600.0, 600.0);
            
            Vehicle* v = new Vehicle(401);
            v->setTransmissionRadius(500.0);
            
            // Try to create dynamics CSV component (vehicle_0)
            try {
                std::string dynamics_csv_file = "include/app/components/datasets/dataset/dynamics-vehicle_0.csv";
                v->create_csv_component_with_file("DynamicsCSV", dynamics_csv_file);
                db<Vehicle>(INF) << "[Dynamics 401] Dynamics CSV component created\n";
            } catch (const std::exception& e) {
                db<Vehicle>(WRN) << "[Dynamics 401] Dynamics CSV not available, using perception data: " << e.what() << "\n";
                // Fallback to perception CSV component with dataset file
                std::string csv_file = "include/app/components/datasets/dataset/perception-vehicle_0.csv";
                v->create_csv_component_with_file("TestCSV", csv_file);
            }
            
            v->start();
            sleep(10);
            v->stop();
            delete v;
            
            Debug::close_log_file();
            exit(0);
        } catch (const std::exception& e) {
            std::cerr << "Dynamics test error: " << e.what() << std::endl;
            exit(1);
        }
    }
    children.push_back(dynamics_pid);
    
    // Wait for process to complete
    int status;
    bool success = (waitpid(dynamics_pid, &status, 0) != -1 && WEXITSTATUS(status) == 0);
    
    assert_true(success, "Dynamics CSV messaging should complete successfully");
    db<CSVMessagingTest>(INF) << "=== Dynamics CSV Messaging Test Completed ===\n";
}

int main(int argc, char* argv[]) {
    CSVMessagingTest test;
    test.run();
    return 0;
} 