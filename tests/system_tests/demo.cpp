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
#include "../../include/api/framework/rsu.h"
#include "../../include/api/framework/leaderKeyStorage.h"
#include "../../include/api/framework/map_config.h"
#include "../../include/app/datatypes.h"
#include "../testcase.h"

#ifndef RSU_RADIUS
#define RSU_RADIUS 400.0
#endif

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
        void testMap1Configuration();
        void testMap2RsuConfiguration();
        void testGenericDemo();
    private:
        int run_demo_with_config(const std::string& config_file);
        void run_vehicle(Vehicle* v, unsigned int vehicle_id);
        void run_rsu(const RSUConfig& rsu_config);
        void setup_rsu_as_leader(unsigned int rsu_id);
        void configure_entity_nic(double transmission_radius_m);
};

Demo::Demo() {
    DEFINE_TEST(testMap1Configuration);
    DEFINE_TEST(testMap2RsuConfiguration);
    DEFINE_TEST(testGenericDemo);
}

void Demo::setUp() {
    // Set up code if needed
}

void Demo::tearDown() {
    // Tear down code if needed
}

void Demo::setup_rsu_as_leader(unsigned int rsu_id) {
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
    
    db<Demo>(INF) << "RSU " << rsu_id << " set as leader with MAC: " 
                  << Ethernet::mac_to_string(rsu_mac) << "\n";
}

void Demo::configure_entity_nic(double transmission_radius_m) {
    // This method is no longer needed as vehicles and RSUs now configure their own radius
    db<Demo>(INF) << "[Config] Transmission radius " << transmission_radius_m << "m will be set by individual entities\n";
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

        db<RSU>(INF) << "[RSU " << rsu_config.id << "] RSU trajectory loaded\n";
        
        if (LocationService::loadTrajectory(rsu_trajectory_file)) {
            db<RSU>(INF) << "[RSU " << rsu_config.id << "] loaded trajectory from " << rsu_trajectory_file << "\n";
        } else {
            db<RSU>(WRN) << "[RSU " << rsu_config.id << "] failed to load trajectory, using config coordinates\n";
            // Set RSU position from config
            LocationService::setCurrentCoordinates(rsu_config.x, rsu_config.y);
        }
        
        // Setup RSU as leader before creating the RSU instance
        setup_rsu_as_leader(rsu_config.id);
        
        // Get transmission radius from config (fallback to RSU_RADIUS if no config)
        double transmission_radius = g_map_config ? g_map_config->get_transmission_radius() : RSU_RADIUS;
        
        // Create RSU with configuration parameters
        RSU* rsu = new RSU(rsu_config.id, rsu_config.unit, rsu_config.broadcast_period, rsu_config.x, rsu_config.y, transmission_radius);
        
        db<RSU>(INF) << "[RSU " << rsu_config.id << "] RSU created with address " 
                     << rsu->address().to_string() << "\n";
        
        // RSU now configures its own transmission radius in constructor
        
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

void Demo::testMap1Configuration() {
    std::string config_file = "config/map_1_config.json";
    
    if (!std::filesystem::exists(config_file)) {
        // Skip test if config file doesn't exist
        return;
    }
    
    int result = run_demo_with_config(config_file);
    assert_equal(0, result, "Map 1 configuration demo should complete successfully");
}

void Demo::testMap2RsuConfiguration() {
    std::string config_file = "config/map_2rsu_config.json";
    
    if (!std::filesystem::exists(config_file)) {
        // Skip test if config file doesn't exist
        return;
    }
    
    int result = run_demo_with_config(config_file);
    assert_equal(0, result, "Map 2 RSU configuration demo should complete successfully");
}

void Demo::testGenericDemo() {
    // Load map configuration - try to find any available config file (THE FIRST ONE IS THE CHOSEN ONE!)
    std::vector<std::string> config_candidates = {
        "config/map_1_config.json",
        "config/map_2rsu_config.json"
    };
    
    std::string config_file;
    for (const auto& candidate : config_candidates) {
        if (std::filesystem::exists(candidate)) {
            config_file = candidate;
            break;
        }
    }
    
    assert_false(config_file.empty(), "At least one configuration file should be available");
    
    int result = run_demo_with_config(config_file);
    assert_equal(0, result, "Generic demo should complete successfully");
}

int Demo::run_demo_with_config(const std::string& config_file) {
    try {
        g_map_config = std::make_unique<MapConfig>(config_file);
        db<Demo>(INF) << "Loaded map configuration from " << config_file << "\n";
    } catch (const std::exception& e) {
        db<Demo>(ERR) << "Error: Could not load config file " << config_file << ": " << e.what() << "\n";
        return -1;
    }
    
    // Get configuration values with safety checks
    unsigned int n_vehicles;
    double transmission_radius;
    std::vector<RSUConfig> rsu_configs;
    
    try {
        n_vehicles = g_map_config->vehicle_config().default_count;
        transmission_radius = g_map_config->get_transmission_radius();
        rsu_configs = g_map_config->get_all_rsu_configs();
        
        if (rsu_configs.empty()) {
            db<Demo>(ERR) << "Error: No RSU configurations found in config file\n";
            return -1;
        }
        
        if (n_vehicles == 0) {
            db<Demo>(ERR) << "Error: Vehicle count is zero in config file\n";
            return -1;
        }
        
    } catch (const std::exception& e) {
        db<Demo>(ERR) << "Error accessing configuration values: " << e.what() << "\n";
        return -1;
    }

    // Ensure log directories exist
    std::string trajectory_dir;
    try {
        trajectory_dir = g_map_config->logging().trajectory_dir;
    } catch (const std::exception& e) {
        db<Demo>(WRN) << "Error accessing trajectory directory config: " << e.what() << "\n";
        trajectory_dir = "tests/logs/trajectories"; // fallback
    }
    
    try {
        std::filesystem::create_directories(trajectory_dir);
        db<Demo>(INF) << "Created trajectory directory: " << trajectory_dir << "\n";
    } catch (const std::exception& e) {
        db<Demo>(WRN) << "Warning: Could not create trajectory directory: " << e.what() << "\n";
    }

    // === STEP 0: Generate trajectories using Python script ===
    db<Demo>(INF) << "Generating trajectory files for " << n_vehicles << " vehicles and " 
                  << rsu_configs.size() << " RSUs\n";
    db<Demo>(INF) << "Using transmission radius: " << transmission_radius << "m for all entities\n";
    
    // Use the trajectory generator script specified in the config
    std::string script_path;
    try {
        script_path = g_map_config->get_trajectory_generator_script();
    } catch (const std::exception& e) {
        db<Demo>(WRN) << "Error accessing trajectory generator script config: " << e.what() << "\n";
        script_path = "scripts/trajectory_generator_map_1.py"; // fallback
    }
    
    std::string python_command = "python3 " + script_path;
    python_command += " --config " + config_file;
    
    db<Demo>(INF) << "Using trajectory generator: " << script_path << "\n";
    
    int trajectory_result = system(python_command.c_str());
    if (trajectory_result != 0) {
        db<Demo>(WRN) << "Warning: Trajectory generation failed or not available, using manual coordinates\n";
    } else {
        db<Demo>(INF) << "Trajectory generation completed successfully\n";
    }

    std::vector<pid_t> children;
    children.reserve(n_vehicles + rsu_configs.size());

    // === STEP 1: Create and start RSU processes first ===
    db<Demo>(INF) << "Creating " << rsu_configs.size() << " RSU processes\n";
    
    for (const auto& rsu_config : rsu_configs) {
        pid_t rsu_pid = fork();
        if (rsu_pid < 0) {
            db<Demo>(ERR) << "[ERROR] failed to fork RSU process for ID " << rsu_config.id << "\n";
            return -1;
        }
        
        if (rsu_pid == 0) { // RSU child process
            std::cout << "[RSU Child " << getpid() << "] creating RSU " << rsu_config.id << std::endl;
            run_rsu(rsu_config);
            exit(0);
        } else { // Parent process
            children.push_back(rsu_pid);
            db<Demo>(INF) << "Created RSU process " << rsu_pid << " for RSU " << rsu_config.id << "\n";
            
            // Give RSU time to start and establish leadership
            sleep(2);
        }
    }
    
    db<Demo>(INF) << "All RSUs started, proceeding with vehicle creation\n";

    // === STEP 2: Create vehicle processes ===
    db<Demo>(INF) << "Creating " << n_vehicles << " vehicle processes\n";
    
    for (unsigned int id = 1; id <= n_vehicles; ++id) {
        // Sleep for 500ms between vehicle creations to stagger startup
        usleep(500000);

        pid_t pid = fork();

        if (pid < 0) {
            db<Demo>(ERR) << "[ERROR] failed to fork vehicle process for ID " << id << "\n";
            db<Demo>(ERR) << "Application terminated.\n";
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
                run_vehicle(v, id);
                
                Debug::close_log_file();
                std::cout << "[Vehicle Child " << getpid() << "] vehicle " << id << " finished execution" << std::endl;
                exit(0);
            } catch (const std::exception& e) {
                std::cerr << "Error in vehicle child process: " << e.what() << std::endl;
                exit(1);
            }
        } else { // Parent process
            children.push_back(pid);
            db<Demo>(INF) << "Created vehicle child process " << pid << " for vehicle " << id << "\n";
        }
    }

    // === STEP 3: Wait for all vehicle processes to complete first ===
    db<Demo>(INF) << "Waiting for all " << n_vehicles << " vehicle processes to complete\n";
    
    bool successful = true;
    int completed_vehicles = 0;
    
    // Wait for all vehicle processes (skip RSU processes)
    for (size_t i = rsu_configs.size(); i < children.size(); ++i) {
        pid_t child_pid = children[i];
        int status;
        if (waitpid(child_pid, &status, 0) == -1) {
            db<Demo>(ERR) << "[ERROR] failed to wait for vehicle child " << child_pid << "\n";
            successful = false;
        } else if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            completed_vehicles++;
            db<Demo>(INF) << "[Parent] Vehicle child " << child_pid << " exited with status " << exit_status << "\n";
            if (exit_status != 0) successful = false;
        } else if (WIFSIGNALED(status)) {
            db<Demo>(WRN) << "[Parent] Vehicle child " << child_pid << " terminated by signal " << WTERMSIG(status) << "\n";
            successful = false;
        } else {
            db<Demo>(WRN) << "[Parent] Vehicle child " << child_pid << " terminated abnormally\n";
            successful = false;
        }
    }

    db<Demo>(INF) << "All vehicles completed: " << completed_vehicles << "/" << n_vehicles << " vehicles finished\n";
    
    // === STEP 4: Signal all RSUs to terminate ===
    db<Demo>(INF) << "Signaling all RSUs to terminate\n";
    for (size_t i = 0; i < rsu_configs.size(); ++i) {
        pid_t rsu_pid = children[i];
        if (kill(rsu_pid, SIGUSR2) == -1) {
            db<Demo>(ERR) << "[ERROR] Failed to signal RSU process " << rsu_pid << ": " << strerror(errno) << "\n";
            successful = false;
        } else {
            db<Demo>(INF) << "Successfully signaled RSU " << rsu_configs[i].id << " to terminate\n";
        }
    }
    
    // === STEP 5: Wait for all RSUs to complete ===
    db<Demo>(INF) << "Waiting for all RSU processes to complete\n";
    for (size_t i = 0; i < rsu_configs.size(); ++i) {
        pid_t rsu_pid = children[i];
        int status;
        if (waitpid(rsu_pid, &status, 0) == -1) {
            db<Demo>(ERR) << "[ERROR] failed to wait for RSU process " << rsu_pid << ": " << strerror(errno) << "\n";
            successful = false;
        } else if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            db<Demo>(INF) << "[Parent] RSU process " << rsu_pid << " exited with status " << exit_status << "\n";
            if (exit_status != 0) successful = false;
        } else if (WIFSIGNALED(status)) {
            db<Demo>(WRN) << "[Parent] RSU process " << rsu_pid << " terminated by signal " << WTERMSIG(status) << "\n";
            successful = false;
        } else {
            db<Demo>(WRN) << "[Parent] RSU process " << rsu_pid << " terminated abnormally\n";
            successful = false;
        }
    }

    db<Demo>(INF) << "Demo completed: " << completed_vehicles << " vehicles and " << rsu_configs.size() << " RSUs finished\n";
    db<Demo>(INF) << (successful ? "Demo completed successfully!" : "Demo terminated with errors!") << "\n";
    
    return successful ? 0 : -1;
}

void Demo::run_vehicle(Vehicle* v, unsigned int vehicle_id) {
    db<Vehicle>(TRC) << "run_vehicle() called!\n";

    // Safety check for global config
    if (!g_map_config) {
        db<Vehicle>(ERR) << "[Vehicle " << vehicle_id << "] Error: g_map_config is null\n";
        return;
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    
    // vehicle_id parameter is passed directly to avoid accessing v->id() after potential deletion
    
    // Use simulation duration from config with safety checks
    unsigned int sim_duration;
    double transmission_radius;
    std::string trajectory_file;
    
    try {
        sim_duration = g_map_config->simulation().duration_s;
        transmission_radius = g_map_config->get_transmission_radius();
        trajectory_file = g_map_config->get_trajectory_file_path("vehicle", vehicle_id);
    } catch (const std::exception& e) {
        db<Vehicle>(ERR) << "[Vehicle " << vehicle_id << "] Error accessing config: " << e.what() << "\n";
        // Use fallback values
        sim_duration = 30;
        transmission_radius = 500.0;
        trajectory_file = "tests/logs/trajectories/vehicle_" + std::to_string(vehicle_id) + "_trajectory.csv";
    }
    
    // Configure vehicle transmission radius from config
    v->setTransmissionRadius(transmission_radius);
    
    if (LocationService::loadTrajectory(trajectory_file)) {
        db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] loaded trajectory from " << trajectory_file << "\n";
    } else {
        db<Vehicle>(WRN) << "[Vehicle " << vehicle_id << "] failed to load trajectory, using default coordinates\n";
        // Set a default position if trajectory loading fails (use first RSU position as default)
        try {
            auto rsu_configs = g_map_config->get_all_rsu_configs();
            if (!rsu_configs.empty()) {
                LocationService::setCurrentCoordinates(rsu_configs[0].x, rsu_configs[0].y);
            } else {
                LocationService::setCurrentCoordinates(500.0, 500.0); // Hardcoded fallback
            }
        } catch (const std::exception& e) {
            db<Vehicle>(ERR) << "[Vehicle " << vehicle_id << "] Error accessing RSU config: " << e.what() << "\n";
            LocationService::setCurrentCoordinates(500.0, 500.0); // Hardcoded fallback
        }
    }

    // Create all basic components for every vehicle (generic approach)
    db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] creating all basic components\n";
    
    // Give every vehicle all basic components
    v->create_component<BasicProducerA>("ProducerA");
    v->create_component<BasicProducerB>("ProducerB");
    v->create_component<BasicConsumerA>("ConsumerA");
    v->create_component<BasicConsumerB>("ConsumerB");
    
    db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] configured with all basic components (Producer-A, Producer-B, Consumer-A, Consumer-B)\n";

    // Start consumers with periodic interest
    auto consumerA = v->get_component<Agent>("ConsumerA");
    auto consumerB = v->get_component<Agent>("ConsumerB");
    
    if (consumerA) {
        // Start ConsumerA with 500ms period using Agent's periodic interest method
        consumerA->start_periodic_interest(static_cast<std::uint32_t>(DataTypes::UNIT_A), Agent::Microseconds(500000));
        db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] ConsumerA started with 500ms period\n";
    }
    
    if (consumerB) {
        // Start ConsumerB with 750ms period using Agent's periodic interest method
        consumerB->start_periodic_interest(static_cast<std::uint32_t>(DataTypes::UNIT_B), Agent::Microseconds(750000));
        db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] ConsumerB started with 750ms period\n";
    }

    // Use randomized behavior for vehicle lifetime
    unsigned int min_lifetime = std::max(5u, sim_duration / 3);  // At least 5s, or 1/3 of sim duration
    unsigned int max_lifetime = std::max(10u, sim_duration);     // At least 10s, or full sim duration
    
    std::uniform_int_distribution<> dist_lifetime(min_lifetime, max_lifetime);
    std::uniform_int_distribution<> start_delay(0, 3);
    int delay = start_delay(gen);
    int lifetime = dist_lifetime(gen);

    db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] components created, starting in " << delay << "s for " << lifetime << "s lifetime\n";
    sleep(delay);
    
    v->start();
    db<Vehicle>(INF) << "[Vehicle " << vehicle_id << "] started\n";
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