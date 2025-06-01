# Beamforming Usage Guide

## Overview

The beamforming implementation provides a complete vehicular communication simulation platform with directional transmission patterns, geographic range limitations, **dynamic trajectory-based positioning**, and **automated scenario generation**. This enables realistic wireless communication simulations for research, development, and performance analysis.

## Key Components

### 1. BeamformingInfo Structure

```cpp
struct BeamformingInfo {
    double sender_latitude;     // Automatically set from LocationService
    double sender_longitude;    // Automatically set from LocationService  
    float beam_center_angle;    // degrees, 0-359.9 (0° = East, 90° = North)
    float beam_width_angle;     // degrees, 360.0 = omnidirectional
    float max_range;            // meters
};
```

### 2. Advanced LocationService with Trajectory Support

The LocationService supports both **trajectory-based positioning** from CSV files and manual coordinate management:

```cpp
LocationService& location = LocationService::getInstance();

// Method 1: Load trajectory from CSV file (recommended for simulations)
bool success = location.loadTrajectory("trajectories/vehicle_1_trajectory.csv");

// Method 2: Manual coordinate setting (fallback or static scenarios)
location.setCurrentCoordinates(latitude, longitude);

// Get coordinates at specific timestamp (for time-synchronized simulations)
double lat, lon;
auto timestamp = std::chrono::milliseconds(5000); // 5 seconds
location.getCurrentCoordinates(lat, lon, timestamp);

// Get coordinates at current system time
location.getCurrentCoordinates(lat, lon);

// Utility methods
bool hasTrajectory = location.hasTrajectory();
auto duration = location.getTrajectoryDuration();
```

**Advanced Capabilities:**
- **CSV Parsing**: Robust parsing with header detection and error handling
- **Temporal Interpolation**: Linear interpolation between trajectory points for smooth movement
- **Thread Safety**: Mutex-protected for concurrent access in multi-process scenarios
- **Fallback Support**: Graceful degradation to manual coordinates if trajectory loading fails
- **Time-based Queries**: Get position at any simulation timestamp

### 3. Trajectory CSV Format

The trajectory files use a simple CSV format compatible with analysis tools:

```csv
timestamp_ms,latitude,longitude
0,-27.596900,-48.548200
100,-27.596850,-48.548150
200,-27.596800,-48.548100
...
```

- **timestamp_ms**: Milliseconds since simulation start (0-based)
- **latitude**: GPS latitude coordinate (WGS84)
- **longitude**: GPS longitude coordinate (WGS84)

### 4. Automated Trajectory Generation

Use the Python script to generate realistic trajectories for any simulation scenario:

```bash
# Basic usage - generates 30 vehicles for 30-second simulation
python3 scripts/trajectory_generator_map_1.py --vehicles 30 --duration 30

# Advanced usage with custom parameters
python3 scripts/trajectory_generator_map_1.py \
    --vehicles 50 \
    --duration 60 \
    --output-dir tests/logs/trajectories \
    --update-interval 100
```

**Generator Features:**
- **Realistic Movement**: 30-60 km/h urban traffic speeds
- **Strategic RSU Placement**: Optimal center position for maximum coverage
- **Geographic Realism**: 1.1km × 1.1km Florianópolis region simulation
- **Dynamic Scaling**: Adjusts automatically to any number of vehicles
- **GPS Noise**: Realistic positioning variance for authentic simulation
- **Edge-to-Edge Movement**: Vehicles traverse the map with random destinations

**Command Line Options:**
```bash
python3 scripts/trajectory_generator_map_1.py [OPTIONS]

Options:
  --vehicles NUM        Number of vehicles (default: 30)
  --duration SECONDS    Simulation duration (default: 30)
  --output-dir PATH     Output directory (default: tests/logs/trajectories)
  --update-interval MS  Update interval in ms (default: 100)
```

### 5. Sending with Beamforming

```cpp
// Method 1: Using Protocol directly
BeamformingInfo beam_info;
beam_info.beam_center_angle = 45.0f;   // Northeast direction
beam_info.beam_width_angle = 90.0f;    // 90° beam width  
beam_info.max_range = 500.0f;          // 500m range

protocol->send(from_addr, to_addr, data, size, beam_info);

// Method 2: Using Communicator
communicator.send(&message, beam_info);

// Method 3: Default omnidirectional (backward compatible)
protocol->send(from_addr, to_addr, data, size);  // 360° beam, 1000m range
```

## Usage Examples

### Example 1: Vehicle with Trajectory-Based Movement

```cpp
#include "api/framework/network.h"
#include "api/network/communicator.h"
#include "api/framework/location_service.h"

// Load vehicle trajectory for dynamic positioning
LocationService& location = LocationService::getInstance();
if (!location.loadTrajectory("tests/logs/trajectories/vehicle_1_trajectory.csv")) {
    // Fallback to manual coordinates
    location.setCurrentCoordinates(37.7749, -122.4194);
}

// Send beamforming message (coordinates automatically filled from trajectory)
Network::Communicator comm(network.channel(), {network.address(), 8001});
BeamformingInfo forward_beam;
forward_beam.beam_center_angle = 0.0f;    // Forward direction
forward_beam.beam_width_angle = 120.0f;   // Wide forward arc
forward_beam.max_range = 300.0f;          // 300m safety range

Network::Message safety_msg("Emergency brake ahead!");
comm.send(&safety_msg, forward_beam);
```

### Example 2: RSU with Static Positioning

```cpp
// RSU loads its static trajectory (same position for all timestamps)
LocationService& location = LocationService::getInstance();
location.loadTrajectory("tests/logs/trajectories/rsu_1000_trajectory.csv");

// RSU broadcasts omnidirectional status messages
BeamformingInfo broadcast_beam; // Default omnidirectional
Network::Message status_msg("RSU Status: Active");
comm.send(&status_msg, broadcast_beam);
```

### Example 3: Time-Synchronized Communication

```cpp
// Get coordinates at specific simulation time
auto simulation_time = std::chrono::milliseconds(10000); // 10 seconds
double lat, lon;
location.getCurrentCoordinates(lat, lon, simulation_time);

// Send directional beam toward intersection
BeamformingInfo intersection_beam;
intersection_beam.beam_center_angle = 45.0f;   // Northeast toward intersection
intersection_beam.beam_width_angle = 60.0f;    // Focused beam
intersection_beam.max_range = 200.0f;          // Intersection communication range

Network::Message traffic_msg("Traffic light changing in 5s");
comm.send(&traffic_msg, intersection_beam);
```

### Example 4: Automated Simulation Setup

The system tests demonstrate complete automation from trajectory generation to execution:

```cpp
// System automatically generates trajectories and loads them
void Demo::run_demo() {
    // 1. Generate trajectories for all vehicles and RSUs
    std::string python_command = "python3 scripts/trajectory_generator_map_1.py";
    python_command += " --vehicles " + std::to_string(n_vehicles);
    python_command += " --duration 30";
    system(python_command.c_str());
    
    // 2. Vehicles automatically load their trajectories during initialization
    // 3. RSU loads its static trajectory
    // 4. Beamforming works seamlessly with dynamic positioning
}
```

## Packet Filtering

The beamforming system automatically filters received packets based on:

1. **Distance Check**: Packets from senders beyond `max_range` are dropped
2. **Beam Direction Check**: For directional beams (`beam_width_angle < 360°`), packets from senders outside the beam pattern are dropped
3. **Early Filtering**: Filtering occurs in `Protocol::update()` before application processing
4. **Dynamic Positioning**: Sender and receiver positions are automatically retrieved from trajectories

### Debug Output

Enable debug output to see filtering decisions:

```
[Protocol] Packet dropped: out of range (1200m > 1000m)
[Protocol] Packet dropped: outside beam (bearing=45, center=90, width=30)
[Vehicle 5] loaded trajectory from tests/logs/trajectories/vehicle_5_trajectory.csv
[RSU 1000] loaded trajectory from tests/logs/trajectories/rsu_1000_trajectory.csv
```

## Geographic Utilities

### Calculate Distance

```cpp
double distance = GeoUtils::haversineDistance(
    sender_lat, sender_lon, 
    receiver_lat, receiver_lon
);
```

### Calculate Bearing  

```cpp
float bearing = GeoUtils::bearing(
    sender_lat, sender_lon,
    receiver_lat, receiver_lon  
);
```

### Check Beam Containment

```cpp
bool in_beam = GeoUtils::isInBeam(bearing, beam_center, beam_width);
```

## Testing Infrastructure

### Integration Tests

The system includes comprehensive test coverage:

```bash
# Run beamforming-specific integration tests
./tests/integration_tests/beamforming_test
```

**Test Coverage:**
- Omnidirectional and directional beam configuration
- Geographic calculations (Haversine distance, bearing)
- Beam containment logic with wraparound support
- Distance filtering verification
- LocationService functionality and thread safety
- Backward compatibility with existing code

### System Tests with Automated Trajectories

```bash
# Run full system demo with automatic trajectory generation
./tests/system_tests/demo
```

**System Test Features:**
- Automatic trajectory generation for all entities
- Multi-process vehicle and RSU simulation
- Dynamic trajectory loading and beamforming integration
- Realistic 30-vehicle, 30-second urban simulation

### Manual Testing

```bash
# Generate custom trajectories for testing
python3 scripts/trajectory_generator_map_1.py --vehicles 5 --duration 10

# Verify generated CSV files
ls tests/logs/trajectories/
cat tests/logs/trajectories/vehicle_1_trajectory.csv
```

## Usage Scenarios

### Scenario 1: Development Testing
```bash
# Quick development testing with small scenario
python3 scripts/trajectory_generator_map_1.py --vehicles 5 --duration 10
./tests/integration_tests/beamforming_test
./tests/system_tests/demo
```

### Scenario 2: Research Simulation
```bash
# Large-scale research simulation
python3 scripts/trajectory_generator_map_1.py --vehicles 100 --duration 300

# Extended research scenario
python3 scripts/trajectory_generator_map_1.py \
    --vehicles 200 \
    --duration 600 \
    --update-interval 50
```

### Scenario 3: Performance Analysis
```cpp
// Load trajectory and analyze beamforming performance
LocationService& location = LocationService::getInstance();
location.loadTrajectory("vehicle_trajectory.csv");

// Test different beam configurations
BeamformingInfo narrow_beam;
narrow_beam.beam_width_angle = 30.0f;  // Narrow, focused
narrow_beam.max_range = 500.0f;

BeamformingInfo wide_beam;  
wide_beam.beam_width_angle = 120.0f;   // Wide coverage
wide_beam.max_range = 300.0f;
```

## Performance Considerations

### Computational Overhead
- **Geographic Calculations**: ~6-11µs per packet for beamforming calculations
- **Trajectory Loading**: One-time CSV parsing at startup (~1-10ms per file)
- **Position Lookup**: Binary search + interpolation (~1-2µs per query)
- **Early Filtering**: Packets dropped at Protocol layer before reaching applications
- **Memory Usage**: ~8 bytes per trajectory point per vehicle

### Storage Requirements
- **30 vehicles, 30 seconds, 100ms interval**: ~300KB total for all CSV files
- **100 vehicles, 300 seconds, 50ms interval**: ~6MB total for all CSV files

### Network Impact
- **Memory Overhead**: Each packet includes 28 additional bytes for BeamformingInfo
- **Backward Compatibility**: Existing code continues to work with default omnidirectional beams

## Architecture Benefits

### Clean Design Principles
- **Layer Separation**: NIC handles Ethernet, Protocol handles beamforming, Applications handle business logic
- **Protocol-Level Filtering**: Early packet dropping at the optimal layer
- **Type Safety**: Protocol layer understands exact packet structures
- **Maintainability**: Beamforming logic centralized in Protocol layer

### Dynamic Capabilities
- **Real-time Positioning**: Trajectory-based coordinate updates during simulation
- **Scalable Testing**: Automated trajectory generation for any scenario size
- **Research Ready**: CSV format compatible with analysis tools and external systems
- **Multi-Map Support**: Foundation for multiple simulation environments

## Future Extensibility

### Planned Map Scenarios
- **Map 1**: Urban grid (current) - 1.1km × 1.1km Florianópolis region
- **Map 2**: Highway scenarios (planned) - Multi-lane highway with on/off ramps
- **Map 3**: Campus environments (planned) - University campus with buildings

### Advanced Features (Future)
- Multiple RSU support with overlapping coverage areas
- Advanced movement patterns (traffic lights, intersections, parking)
- Real-world trajectory import from GPS traces
- Dynamic beamforming pattern optimization

### Research Applications
- V2V communication range analysis under realistic mobility
- Beamforming pattern optimization for different scenarios
- RSU placement strategy evaluation
- Network performance analysis under various traffic conditions

## Integration with Existing Code

The beamforming implementation maintains full backward compatibility:

```cpp
// Existing code continues to work unchanged
protocol->send(from, to, data, size);  // Uses default BeamformingInfo

// Enhanced code with trajectory support
LocationService::getInstance().loadTrajectory("trajectory.csv");
BeamformingInfo beam_info;
protocol->send(from, to, data, size, beam_info);
```

## Coordinate System

- **Latitude**: Positive = North, Negative = South
- **Longitude**: Positive = East, Negative = West  
- **Bearing**: 0° = East, 90° = North, 180° = West, 270° = South
- **Distance**: Calculated using Haversine formula for great-circle distance
- **Timestamps**: Milliseconds since simulation start (0-based)
- **Geographic Reference**: WGS84 coordinate system 