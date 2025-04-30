# Component Architecture

This document describes the Component base class architecture, which provides a standardized foundation for all vehicle components in the system.

## Overview

The Component class serves as the base class for all specialized vehicle components. It provides:

1. Thread management for concurrent execution
2. Standardized communication interface
3. Lifecycle management (start/stop)
4. Error handling and logging capabilities

## Class Definition

The Component class is defined in `component.h` and has the following structure:

```cpp
class Component {
public:
    // Constructor
    Component(Vehicle* vehicle, const std::string& name, TheProtocol* protocol, TheAddress address);

    // Virtual destructor
    virtual ~Component() = default;

    // Lifecycle methods
    virtual void start();
    virtual void stop();
    
    // Pure virtual run method (must be implemented by derived classes)
    virtual void run() = 0;

    // Accessors
    bool running() const;
    const std::string& getName();
    Vehicle* vehicle() const;
    std::ofstream* log_file();

    // Communication methods
    int send(const TheAddress& destination, const void* data, unsigned int size);
    int receive(void* data, unsigned int max_size, TheAddress* source_address = nullptr);

protected:
    // Helper function for thread creation
    static void* thread_entry_point(void* arg);

    // Common members
    Vehicle* _vehicle;
    std::string _name;
    std::atomic<bool> _running;
    pthread_t _thread;
    std::unique_ptr<TheCommunicator> _communicator;
    TheProtocol* _protocol;
    std::ofstream _log_file;

    // Logging methods
    void open_log_file(const std::string& filename_prefix);
    void close_log_file();
};
```

## Key Features

### Thread Management

The Component class manages a dedicated thread for each component:

- `start()`: Creates a new thread that executes the component's `run()` method
- `stop()`: Signals the thread to stop and joins it
- Thread safety using atomic variables and proper synchronization
- Error handling for thread creation failures

### Communication Interface

Components communicate with each other using a standardized interface:

- Each component has its own `Communicator` instance
- `send()`: Sends a message to a specified destination address
- `receive()`: Receives messages addressed to this component
- Messages can be sent to specific components or broadcast to all

### Lifecycle Management

Components follow a defined lifecycle:

1. **Creation**: Component is instantiated with references to its Vehicle and Protocol
2. **Initialization**: Component initializes internal state and opens log files
3. **Starting**: Component thread is created and begins executing the `run()` method
4. **Running**: Component executes its main logic in the `run()` method
5. **Stopping**: Component is signaled to stop, cleanups resources, and joins its thread
6. **Destruction**: Component instance is destroyed

### Error Handling and Logging

The Component class provides comprehensive error handling and logging:

- Each component creates a dedicated log file for its activities
- Integration with the debug system for different verbosity levels
- Exception handling in thread execution
- Error reporting for communication failures

## Creating Specialized Components

To create a specialized component:

1. Inherit from the Component base class
2. Implement the pure virtual `run()` method with the component's main logic
3. Optionally override other virtual methods as needed

Example:

```cpp
class SensorComponent : public Component {
public:
    SensorComponent(Vehicle* vehicle, const std::string& name, TheProtocol* protocol, TheAddress address)
        : Component(vehicle, name, protocol, address) {
        // Sensor-specific initialization
    }

    void run() override {
        while (running()) {
            // Read sensor data
            std::string data = read_sensor();
            
            // Send data to destination
            send(destination_address, data.c_str(), data.size());
            
            // Wait for next cycle
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

private:
    std::string read_sensor() {
        // Sensor-specific implementation
        return "Sensor reading";
    }
};
```

## Specialized Component Types

The system includes several specialized component types:

1. **LiDAR Component**: Generates and transmits point cloud data
   - Simulates LiDAR sensor readings
   - Sends point cloud data to local ECU and broadcast addresses
   - Configurable scan rate and point cloud density

2. **Camera Component**: Simulates video frame capture
   - Generates simulated image frames
   - Includes timestamp and encoding information
   - Configurable frame rate and resolution

3. **ECU Component**: Electronic Control Unit for vehicle management
   - Receives data from sensors
   - Processes and aggregates sensor information
   - Makes vehicle control decisions

4. **INS Component**: Inertial Navigation System
   - Provides position, velocity, and orientation data
   - Simulates GPS and IMU sensors
   - Fusion of multiple navigation sources

5. **Battery Component**: Battery status monitoring
   - Reports battery level and health
   - Simulates battery discharge
   - Detects and reports critical conditions

## Communication Patterns

Components can communicate using several patterns:

1. **Point-to-Point**: Direct communication between two components
   ```cpp
   send(target_component_address, data, size);
   ```

2. **Broadcast**: Send to all components
   ```cpp
   send(broadcast_address, data, size);
   ```

3. **Local Communication**: Communication between components in the same vehicle
   ```cpp
   send(TheAddress(vehicle()->address().paddr(), target_port), data, size);
   ```

4. **Remote Communication**: Communication with components in other vehicles
   ```cpp
   send(TheAddress(remote_mac_address, target_port), data, size);
   ```

## Thread Safety Considerations

When implementing components, consider these thread safety guidelines:

1. Access to shared resources must be protected with mutexes
2. Use atomic variables for flags and counters
3. Avoid data races when accessing component state
4. Be mindful of deadlocks when acquiring multiple locks
5. Use thread-safe communication patterns provided by the framework

## Best Practices

1. **Keep the run() method clean**: Delegate complex logic to helper methods
2. **Handle errors gracefully**: Catch exceptions and recover when possible
3. **Log relevant information**: Use appropriate debug levels for different events
4. **Check running() frequently**: Allow clean shutdown by checking the running flag
5. **Implement clean shutdown**: Release resources properly when stopping

## Performance Considerations

1. **Message Size**: Keep messages small to avoid fragmentation
2. **Processing Time**: Avoid long computations in the component thread
3. **Memory Management**: Be mindful of memory allocations and deallocations
4. **Communication Frequency**: Balance between update rate and network load
5. **Thread Contention**: Minimize lock contention between components

## Example Usage

Here's a complete example of creating and using a custom component:

```cpp
// 1. Define your component
class TemperatureComponent : public Component {
public:
    TemperatureComponent(Vehicle* vehicle, TheProtocol* protocol, TheAddress address)
        : Component(vehicle, "Temperature", protocol, address),
          _temperature(20.0) {
        open_log_file("temp_log");
    }

    void run() override {
        while (running()) {
            // Simulate temperature reading
            _temperature += (rand() % 100 - 50) / 100.0;
            
            // Create message
            std::string msg = "Temperature: " + std::to_string(_temperature) + "°C";
            
            // Send to ECU component
            TheAddress ecu_address(vehicle()->address().paddr(), 2); // Assuming ECU is port 2
            send(ecu_address, msg.c_str(), msg.size());
            
            // Wait for next reading
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

private:
    double _temperature;
};

// 2. Add to vehicle in main program
int main() {
    // Create vehicle and communication stack
    TheNIC* nic = new TheNIC(/* ... */);
    TheProtocol* protocol = new TheProtocol(nic);
    Vehicle* vehicle = new Vehicle(1, nic, protocol);
    
    // Create component with next available address
    TheAddress temp_addr = vehicle->next_component_address();
    auto temp_component = std::make_unique<TemperatureComponent>(vehicle, protocol, temp_addr);
    
    // Add component to vehicle
    vehicle->add_component(std::move(temp_component));
    
    // Start vehicle and components
    vehicle->start();
    
    // Wait for termination signal
    // ...
    
    // Stop vehicle and components
    vehicle->stop();
    
    return 0;
}
``` 