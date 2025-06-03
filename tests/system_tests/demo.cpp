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

#include "../../include/app/vehicle.h"
#include "../../include/api/util/debug.h"
#include "../../include/app/components/basic_producer_a.h"
#include "../../include/app/components/basic_consumer_a.h"
#include "../../include/app/components/basic_producer_b.h"
#include "../../include/app/components/basic_consumer_b.h"
#include "../../include/api/framework/rsu.h"
#include "../../include/api/framework/leaderKeyStorage.h"
#include "../../include/api/framework/map_config.h"
#include "../testcase.h"
#include "../test_utils.h"

// Global flag for RSU termination - volatile for signal safety
volatile sig_atomic_t rsu_should_terminate = 0;

// Global configuration
static std::unique_ptr<MapConfig> g_map_config;

// Signal handler for RSU termination
void rsu_signal_handler(int signal) {
    if (signal == SIGUSR2) {
        rsu_should_terminate = 1;
        // Write to stderr for immediate output (safer in signal handler)
        write(STDERR_FILENO, "[RSU Signal] Received SIGUSR2, setting termination flag\n", 56);
    }
}

// Helper function to set up a vehicle log directory
std::string setup_log_directory(unsigned int vehicle_id) {
    std::string log_file;
    std::error_code ec;
    
    // Use simple trajectory directory since we simplified logging config
    std::string base_log_dir = g_map_config ? g_map_config->logging().trajectory_dir + "/.." : "tests/logs";
    
    // Try in priority order: Docker logs dir, config logs dir, current dir
    if (std::filesystem::exists("/app/logs")) {
        std::string vehicle_dir = "/app/logs/vehicle_" + std::to_string(vehicle_id);
        std::filesystem::create_directory(vehicle_dir, ec);
        
        if (!ec) {
            return vehicle_dir + "/vehicle_" + std::to_string(vehicle_id) + ".log";
        }
    }
    
    // Try config logs directory
    std::string test_logs_dir = base_log_dir + "/vehicle_" + std::to_string(vehicle_id);
    
    try {
        std::filesystem::create_directories(test_logs_dir);
        return test_logs_dir + "/vehicle_" + std::to_string(vehicle_id) + ".log";
    } catch (...) {
        // Fallback to base logs without vehicle subfolder
        return base_log_dir + "/vehicle_" + std::to_string(vehicle_id) + ".log";
    }
}

// Helper function to set up RSU log directory
std::string setup_rsu_log_directory(unsigned int rsu_id) {
    std::string log_file;
    std::error_code ec;
    
    // Use simple trajectory directory since we simplified logging config
    std::string base_log_dir = g_map_config ? g_map_config->logging().trajectory_dir + "/.." : "tests/logs";
    
    // Try in priority order: Docker logs dir, config logs dir, current dir
    if (std::filesystem::exists("/app/logs")) {
        std::string rsu_dir = "/app/logs/rsu_" + std::to_string(rsu_id);
        std::filesystem::create_directory(rsu_dir, ec);
        
        if (!ec) {
            return rsu_dir + "/rsu_" + std::to_string(rsu_id) + ".log";
        }
    }
    
    // Try config logs directory
    std::string test_logs_dir = base_log_dir + "/rsu_" + std::to_string(rsu_id);
    
    try {
        std::filesystem::create_directories(test_logs_dir);
        return test_logs_dir + "/rsu_" + std::to_string(rsu_id) + ".log";
    } catch (...) {
        // Fallback to base logs without RSU subfolder
        return base_log_dir + "/rsu_" + std::to_string(rsu_id) + ".log";
    }
}

class Demo: public TestCase {
    public:
        Demo();
        ~Demo() = default;

        void setUp() override;
        void tearDown() override;

        /* TESTS */
        int run_demo();
    private:
        void run_vehicle(Vehicle* v);
        void run_rsu(const RSUConfig& rsu_config);
        void setup_rsu_as_leader(unsigned int rsu_id);
        void configure_entity_nic(double transmission_radius_m);
};

Demo::Demo() {
    TEST_INIT("system_demo");
    TEST_LOG("Application started!");
    DEFINE_TEST(run_demo);
}

void Demo::setUp() {
    // Set up code if needed
}

void Demo::tearDown() {
    // Tear down code if needed
    TEST_INIT("test teardown");
    TEST_LOG("Demo test case completed.");
}

void Demo::setup_rsu_as_leader(unsigned int rsu_id) {
    TEST_INIT("Setting up RSU as leader");
    // Create a high-age unique key for the RSU to ensure it becomes leader
    MacKeyType rsu_key;
    rsu_key.fill(0);
    // Use RSU ID in the key to make it unique
    rsu_key[0] = (rsu_id >> 8) & 0xFF;
    rsu_key[1] = rsu_id & 0xFF;
    rsu_key[2] = 0xAA; // Marker for RSU
    rsu_key[3] = 0xBB;
    
    // Create RSU MAC address (similar to how Network creates vehicle MACs)
    Ethernet::Address rsu_mac;
    rsu_mac.bytes[0] = 0x02; // Locally administered
    rsu_mac.bytes[1] = 0x00;
    rsu_mac.bytes[2] = 0x00;
    rsu_mac.bytes[3] = 0x00;
    rsu_mac.bytes[4] = (rsu_id >> 8) & 0xFF;
    rsu_mac.bytes[5] = rsu_id & 0xFF;
    
    // Set RSU as leader in LeaderKeyStorage
    auto& storage = LeaderKeyStorage::getInstance();
    storage.setLeaderId(rsu_mac);
    storage.setGroupMacKey(rsu_key);
    
    TEST_LOG("RSU " + std::to_string(rsu_id) + " set as leader with MAC: " + 
             Ethernet::mac_to_string(rsu_mac));
}

void Demo::configure_entity_nic(double transmission_radius_m) {
    // Configure transmission radius for entities
    // Note: This requires access to NIC, which may need API modifications
    db<Demo>(INF) << "[Config] Setting transmission radius to " << transmission_radius_m << "m\n";
    
    // TODO: Implement actual NIC radius setting when API exposes NIC access
}

void Demo::run_rsu(const RSUConfig& rsu_config) {
    try {
        // Set up signal handler for graceful termination
        signal(SIGUSR2, rsu_signal_handler);
        
        // Set up logging for RSU process
        std::string log_file = setup_rsu_log_directory(rsu_config.id);
        Debug::set_log_file(log_file);
        
        db<RSU>(INF) << "[RSU " << rsu_config.id << "] Starting RSU process\n";
        
        // Load trajectory for RSU (static position)
        std::string rsu_trajectory_file = g_map_config->get_trajectory_file_path("rsu", rsu_config.id);
        
        if (LocationService::loadTrajectory(rsu_trajectory_file)) {
            db<RSU>(INF) << "[RSU " << rsu_config.id << "] loaded trajectory from " << rsu_trajectory_file << "\n";
        } else {
            db<RSU>(WRN) << "[RSU " << rsu_config.id << "] failed to load trajectory, using config coordinates\n";
            // Set RSU position from config
            LocationService::setCurrentCoordinates(rsu_config.lat, rsu_config.lon);
        }
        
        // Setup RSU as leader before creating the RSU instance
        setup_rsu_as_leader(rsu_config.id);
        
        // Create RSU with configuration parameters
        RSU* rsu = new RSU(rsu_config.id, rsu_config.unit, rsu_config.broadcast_period);
        
        db<RSU>(INF) << "[RSU " << rsu_config.id << "] RSU created with address " 
                     << rsu->address().to_string() << "\n";
        
        // Configure transmission radius using simplified config
        if (g_map_config) {
            configure_entity_nic(g_map_config->get_transmission_radius());
        }
        
        // Start the RSU
        rsu->start();
        db<RSU>(INF) << "[RSU " << rsu_config.id << "] RSU started, broadcasting every " 
                     << rsu_config.broadcast_period.count() << "ms\n";
        
        // Wait for signal to stop instead of fixed duration
        db<RSU>(INF) << "[RSU " << rsu_config.id << "] Waiting for all vehicles to complete...\n";
        while (!rsu_should_terminate) {
            pause(); // Wait for signal - more responsive than sleep(1)
        }
        
        db<RSU>(INF) << "[RSU " << rsu_config.id << "] Received termination signal, stopping RSU\n";
        rsu->stop();
        delete rsu;
        
        Debug::close_log_file();
        db<RSU>(INF) << "[RSU " << rsu_config.id << "] RSU process finished\n";
        
    } catch (const std::exception& e) {
        std::cerr << "[RSU " << rsu_config.id << "] Error in RSU process: " << e.what() << std::endl;
        exit(1);
    }
}

int Demo::run_demo() {
    TEST_INIT("the demo test case");
    
    // Load map configuration
    std::string config_file = "config/map_1_config.json";
    try {
        g_map_config = std::make_unique<MapConfig>(config_file);
        TEST_LOG("Loaded map configuration from " + config_file);
    } catch (const std::exception& e) {
        TEST_LOG("Warning: Could not load config file " + config_file + ": " + e.what());
        TEST_LOG("Using default hardcoded values");
        // Continue with defaults
    }
    
    // Get configuration values (with fallbacks)
    unsigned int n_vehicles = g_map_config ? g_map_config->vehicle_config().default_count : 10;
    double transmission_radius = g_map_config ? g_map_config->get_transmission_radius() : 500.0;
    
    RSUConfig rsu_config;
    if (g_map_config) {
        const auto& cfg_rsu = g_map_config->rsu_config();
        rsu_config.id = cfg_rsu.id;
        rsu_config.unit = cfg_rsu.unit;
        rsu_config.broadcast_period = cfg_rsu.broadcast_period;
        rsu_config.lat = cfg_rsu.lat;
        rsu_config.lon = cfg_rsu.lon;
    } else {
        // Fallback RSU configuration
        rsu_config.id = 1000;
        rsu_config.unit = 999;
        rsu_config.broadcast_period = std::chrono::milliseconds(250);
        rsu_config.lat = -27.5919;
        rsu_config.lon = -48.5432;
    }

    // Ensure log directories exist
    std::string trajectory_dir = g_map_config ? g_map_config->logging().trajectory_dir : "tests/logs/trajectories";
    
    try {
        std::filesystem::create_directories(trajectory_dir);
        TEST_LOG("Created trajectory directory: " + trajectory_dir);
    } catch (const std::exception& e) {
        TEST_LOG("Warning: Could not create trajectory directory: " + std::string(e.what()));
    }

    // === STEP 0: Generate trajectories using Python script ===
    TEST_LOG("Generating trajectory files for " + std::to_string(n_vehicles) + " vehicles and 1 RSU");
    TEST_LOG("Using transmission radius: " + std::to_string(transmission_radius) + "m for all entities");
    
    std::string python_command;
    if (g_map_config) {
        // Let Python script read configuration independently to avoid override conflicts
        python_command = "python3 scripts/trajectory_generator_map_1.py";
        python_command += " --config " + config_file;
        // NOTE: Don't pass --vehicles here - let Python use config values directly
    } else {
        // Fallback to old command style when no config is available
        python_command = "python3 scripts/trajectory_generator_map_1.py";
        python_command += " --vehicles " + std::to_string(n_vehicles);
        python_command += " --duration 30";
        python_command += " --output-dir " + trajectory_dir;
        python_command += " --update-interval 100";
    }
    
    int trajectory_result = system(python_command.c_str());
    if (trajectory_result != 0) {
        TEST_LOG("Warning: Trajectory generation failed or not available, using manual coordinates");
    } else {
        TEST_LOG("Trajectory generation completed successfully");
    }

    std::vector<pid_t> children;
    children.reserve(n_vehicles + 1); // +1 for RSU

    // === STEP 1: Create and start RSU process first ===
    TEST_LOG("Creating RSU process (ID: " + std::to_string(rsu_config.id) + ")");
    
    pid_t rsu_pid = fork();
    if (rsu_pid < 0) {
        TEST_LOG("[ERROR] failed to fork RSU process");
        return -1;
    }
    
    if (rsu_pid == 0) { // RSU child process
        std::cout << "[RSU Child " << getpid() << "] creating RSU " << rsu_config.id << std::endl;
        run_rsu(rsu_config);
        exit(0);
    } else { // Parent process
        children.push_back(rsu_pid);
        TEST_LOG("Created RSU process " + std::to_string(rsu_pid) + " for RSU " + std::to_string(rsu_config.id));
        
        // Give RSU time to start and establish leadership
        sleep(2);
        TEST_LOG("RSU startup delay completed, proceeding with vehicle creation");
    }

    // === STEP 2: Create vehicle processes ===
    TEST_LOG("Creating " + std::to_string(n_vehicles) + " vehicle processes");
    
    for (unsigned int id = 1; id <= n_vehicles; ++id) {
        // Sleep for 500ms between vehicle creations to stagger startup
        usleep(500000);

        pid_t pid = fork();

        if (pid < 0) {
            TEST_LOG("[ERROR] failed to fork vehicle process for ID " + std::to_string(id));
            TEST_LOG("Application terminated.");
            return -1;
        }

        if (pid == 0) { // Vehicle child process
            try {
                // Set up logging for child process
                std::string log_file = setup_log_directory(id);
                Debug::set_log_file(log_file);
                
                // Create and run vehicle
                std::cout << "[Vehicle Child " << getpid() << "] creating vehicle " << id << std::endl;
                Vehicle* v = new Vehicle(id);
                run_vehicle(v);
                
                Debug::close_log_file();
                std::cout << "[Vehicle Child " << getpid() << "] vehicle " << id << " finished execution" << std::endl;
                exit(0);
            } catch (const std::exception& e) {
                std::cerr << "Error in vehicle child process: " << e.what() << std::endl;
                exit(1);
            }
        } else { // Parent process
            children.push_back(pid);
            TEST_LOG("Created vehicle child process " + std::to_string(pid) + " for vehicle " + std::to_string(id));
        }
    }

    // === STEP 3: Wait for all vehicle processes to complete first ===
    TEST_LOG("Waiting for all " + std::to_string(n_vehicles) + " vehicle processes to complete");
    
    bool successful = true;
    int completed_vehicles = 0;
    
    // Wait for all vehicle processes (excluding RSU)
    for (size_t i = 1; i < children.size(); ++i) { // Skip RSU (index 0)
        pid_t child_pid = children[i];
        int status;
        if (waitpid(child_pid, &status, 0) == -1) {
            TEST_LOG("[ERROR] failed to wait for vehicle child " + std::to_string(child_pid));
            successful = false;
        } else if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            completed_vehicles++;
            TEST_LOG("[Parent] Vehicle child " + std::to_string(child_pid) + " exited with status " + std::to_string(exit_status));
            if (exit_status != 0) successful = false;
        } else if (WIFSIGNALED(status)) {
            TEST_LOG("[Parent] Vehicle child " + std::to_string(child_pid) + " terminated by signal " + std::to_string(WTERMSIG(status)));
            successful = false;
        } else {
            TEST_LOG("[Parent] Vehicle child " + std::to_string(child_pid) + " terminated abnormally");
            successful = false;
        }
    }

    TEST_LOG("All vehicles completed: " + std::to_string(completed_vehicles) + "/" + std::to_string(n_vehicles) + " vehicles finished");
    
    // === STEP 4: Signal RSU to terminate ===
    TEST_LOG("Signaling RSU to terminate");
    if (kill(rsu_pid, SIGUSR2) == -1) {
        TEST_LOG("[ERROR] Failed to signal RSU process: " + std::string(strerror(errno)));
        successful = false;
    } else {
        TEST_LOG("Successfully signaled RSU to terminate");
    }
    
    // === STEP 5: Wait for RSU to complete ===
    TEST_LOG("Waiting for RSU process to complete");
    int status;
    if (waitpid(rsu_pid, &status, 0) == -1) {
        TEST_LOG("[ERROR] failed to wait for RSU process " + std::to_string(rsu_pid) + ": " + std::string(strerror(errno)));
        successful = false;
    } else if (WIFEXITED(status)) {
        int exit_status = WEXITSTATUS(status);
        TEST_LOG("[Parent] RSU process " + std::to_string(rsu_pid) + " exited with status " + std::to_string(exit_status));
        if (exit_status != 0) successful = false;
    } else if (WIFSIGNALED(status)) {
        TEST_LOG("[Parent] RSU process " + std::to_string(rsu_pid) + " terminated by signal " + std::to_string(WTERMSIG(status)));
        successful = false;
    } else {
        TEST_LOG("[Parent] RSU process " + std::to_string(rsu_pid) + " terminated abnormally");
        successful = false;
    }

    TEST_LOG("Demo completed: " + std::to_string(completed_vehicles) + " vehicles and 1 RSU finished");
    TEST_LOG(successful ? "Application completed successfully!" : "Application terminated with errors!");
    
    return successful ? 0 : -1;
}

void Demo::run_vehicle(Vehicle* v) {
    db<Vehicle>(TRC) << "run_vehicle() called!\n";

    std::random_device rd;
    std::mt19937 gen(rd());
    
    unsigned int vehicle_id = v->id(); // Store ID before potential deletion
    
    // Use simple vehicle lifetime configuration (simplified - no complex config)
    unsigned int min_lifetime = 3;  // Simple fallback
    unsigned int max_lifetime = 7;  // Simple fallback
    
    std::uniform_int_distribution<> dist_lifetime(min_lifetime, max_lifetime);
    std::uniform_int_distribution<> start_delay(0, 3);
    int delay = start_delay(gen);
    int lifetime = dist_lifetime(gen);
    
    // Configure transmission radius using simplified config
    if (g_map_config) {
        configure_entity_nic(g_map_config->get_transmission_radius());
    }
    
    // Load trajectory for this vehicle
    std::string trajectory_file;
    if (g_map_config) {
        trajectory_file = g_map_config->get_trajectory_file_path("vehicle", vehicle_id);
    } else {
        trajectory_file = "tests/logs/trajectories/vehicle_" + std::to_string(vehicle_id) + "_trajectory.csv";
    }
    
    if (LocationService::loadTrajectory(trajectory_file)) {
        db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] loaded trajectory from " << trajectory_file << "\n";
    } else {
        db<Vehicle>(WRN) << "[Vehicle " << vehicle_id << "] failed to load trajectory, using default coordinates\n";
        // Set a default position if trajectory loading fails (use RSU position as center)
        LocationService::setCurrentCoordinates(-27.5919, -48.5432); // RSU position as default
    }

    // Create simple components based on vehicle ID
    db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] creating components\n";
    
    // Even ID vehicles produce A and consume B
    // Odd ID vehicles produce B and consume A
    if (vehicle_id % 2 == 0) {
        v->create_component<BasicProducerA>("ProducerA");
        v->create_component<BasicConsumerB>("ConsumerB");
    } else {
        v->create_component<BasicProducerB>("ProducerB");
        v->create_component<BasicConsumerA>("ConsumerA");
    }

    db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] components created, starting in " << delay << "s\n";

    sleep(delay); // Simulate startup delay

    db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] starting vehicle after " << delay << "s delay\n";

    // Start the vehicle
    v->start();
    db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] started for " << lifetime << "s lifetime\n";

    // Wait for vehicle lifetime
    sleep(lifetime);
    db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] lifetime ended, stopping\n";

    try {
        // Stop vehicle and clean up
        v->stop();
        db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] stopped, cleaning up\n";
        delete v;
        db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] terminated cleanly\n";
    } catch (const std::exception& e) {
        db<Vehicle>(ERR) << "[Vehicle " << vehicle_id << "] Exception during cleanup: " << e.what() << "\n";
    } catch (...) {
        db<Vehicle>(ERR) << "[Vehicle " << vehicle_id << "] Unknown error during cleanup\n";
    }
}

int main(int argc, char* argv[]) {
    Demo demo;
    demo.run();

    return 0;
}