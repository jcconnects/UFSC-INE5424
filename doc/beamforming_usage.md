# Beamforming Usage Guide

## Overview

The beamforming implementation allows applications to send packets with directional transmission patterns and geographic range limitations. This enables simulation of realistic wireless communication scenarios with **dynamic vehicle trajectories** and **static RSU positioning**.

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

### 2. Enhanced LocationService

The LocationService now supports **trajectory-based positioning** from CSV files:

```cpp
LocationService& location = LocationService::getInstance();

// Method 1: Load trajectory from CSV file (recommended)
bool success = location.loadTrajectory("trajectories/vehicle_1_trajectory.csv");

// Method 2: Manual coordinate setting (fallback)
location.setCurrentCoordinates(latitude, longitude);

// Get coordinates at specific timestamp
double lat, lon;
auto timestamp = std::chrono::milliseconds(5000); // 5 seconds
location.getCurrentCoordinates(lat, lon, timestamp);

// Get coordinates at current system time
location.getCurrentCoordinates(lat, lon);
```

### 3. Trajectory CSV Format

The trajectory files use a simple CSV format:

```csv
timestamp_ms,latitude,longitude
0,-27.596900,-48.548200
100,-27.596850,-48.548150
200,-27.596800,-48.548100
...
```

- **timestamp_ms**: Milliseconds since simulation start
- **latitude**: GPS latitude coordinate
- **longitude**: GPS longitude coordinate

### 4. Trajectory Generation

Use the Python script to generate realistic trajectories:

```bash
# Generate trajectories for 30 vehicles, 30-second simulation
python3 scripts/trajectory_generator_map_1.py --vehicles 30 --duration 30

# Custom configuration
python3 scripts/trajectory_generator_map_1.py \
    --vehicles 50 \
    --duration 60 \
    --output-dir tests/logs/trajectories \
    --update-interval 100
```

**Script Features:**
- Generates realistic vehicle movements (30-60 km/h urban speeds)
- Places RSU at map center for optimal coverage
- Creates 1.1km × 1.1km urban area simulation (Florianópolis region)
- Adjusts dynamically to any number of vehicles
- Includes GPS noise for realistic positioning

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

// Load vehicle trajectory
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
forward_beam.max_range = 300.0f;          // 300m range

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
intersection_beam.max_range = 200.0f;          // Intersection range

Network::Message traffic_msg("Traffic light changing in 5s");
comm.send(&traffic_msg, intersection_beam);
```

### Example 4: Integration with System Tests

The system tests automatically generate and use trajectories:

```cpp
// In demo.cpp - trajectories are automatically generated
void Demo::run_demo() {
    // Generate trajectories for all vehicles and RSUs
    std::string python_command = "python3 scripts/trajectory_generator_map_1.py";
    python_command += " --vehicles " + std::to_string(n_vehicles);
    python_command += " --duration 30";
    system(python_command.c_str());
    
    // Vehicles automatically load their trajectories
    // RSU loads its static trajectory
    // Beamforming works with dynamic positioning
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

## Performance Considerations

- **Computational Cost**: Geographic calculations add ~6-11µs per packet
- **Early Filtering**: Packets are filtered at Protocol layer before reaching applications
- **Memory Overhead**: Each packet includes 28 additional bytes for BeamformingInfo
- **Trajectory Loading**: CSV parsing occurs once at startup, minimal runtime overhead
- **Backward Compatibility**: Existing code continues to work with default omnidirectional beams

## Trajectory Generator Features

### Map 1 (Urban Grid)

- **Area**: 1.1km × 1.1km urban simulation
- **Location**: Florianópolis, Brazil region
- **Vehicle Speed**: 30-60 km/h (realistic urban traffic)
- **RSU Placement**: Optimal center position for maximum coverage
- **Update Interval**: 100ms (configurable)
- **GPS Noise**: Realistic positioning variance

### Command Line Options

```bash
python3 scripts/trajectory_generator_map_1.py [OPTIONS]

Options:
  --vehicles NUM        Number of vehicles (default: 30)
  --duration SECONDS    Simulation duration (default: 30)
  --output-dir PATH     Output directory (default: tests/logs/trajectories)
  --update-interval MS  Update interval in ms (default: 100)
```

### Future Maps

The trajectory generator is designed to support multiple maps:
- `trajectory_generator_map_1.py` - Urban grid (current)
- `trajectory_generator_map_2.py` - Highway scenario (future)
- `trajectory_generator_map_3.py` - Campus environment (future)

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

## Testing

### Integration Tests

```bash
# Run beamforming-specific tests
./tests/integration_tests/beamforming_test

# Run full system demo with trajectories
./tests/system_tests/demo
```

### Manual Testing

```bash
# Generate custom trajectories
python3 scripts/trajectory_generator_map_1.py --vehicles 5 --duration 10

# Verify CSV files
ls tests/logs/trajectories/
cat tests/logs/trajectories/vehicle_1_trajectory.csv
```

## Coordinate System

- **Latitude**: Positive = North, Negative = South
- **Longitude**: Positive = East, Negative = West  
- **Bearing**: 0° = East, 90° = North, 180° = West, 270° = South
- **Distance**: Calculated using Haversine formula for great-circle distance
- **Timestamps**: Milliseconds since simulation start (0-based) 