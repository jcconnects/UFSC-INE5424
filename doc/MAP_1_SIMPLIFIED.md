# Map 1 - Simplified Implementation

## Overview

The Map 1 implementation has been simplified to focus on the core collision domain functionality with straightforward trajectory generation. This approach prioritizes clarity and maintainability over complex vehicle behaviors.

## Key Simplifications

### 🎯 **Single Transmission Radius**
- **All entities** (vehicles and RSUs) use the same configurable transmission radius (default: 500m)
- No more complex vehicle type classifications (urban/highway/emergency)
- Simplified collision domain logic: `distance <= transmission_radius`

### 📍 **Waypoint-Based Trajectories**
- Vehicles follow **straight-line paths** between predefined waypoints
- Four basic routes crossing the RSU's collision domain:
  - `west_to_east`: Horizontal crossing from west to east
  - `east_to_west`: Horizontal crossing from east to west  
  - `north_to_south`: Vertical crossing from north to south
  - `south_to_north`: Vertical crossing from south to north

### 🗺️ **Simple Map Layout**
```
    north_entry (-27.5869, -48.5432)
           ↓
west_entry ═══════ RSU ═══════ east_exit
(-27.5919, -48.5482)  (-27.5919, -48.5432)  (-27.5919, -48.5382)
           ↑
    south_exit (-27.5969, -48.5432)
```

## Configuration Structure

### Simplified JSON Configuration (`config/map_1_config.json`)
```json
{
  "simulation": {
    "duration_s": 30,
    "update_interval_ms": 100,
    "default_transmission_radius_m": 500
  },
  "rsu": {
    "id": 1000,
    "position": {"lat": -27.5919, "lon": -48.5432},
    "unit": 999,
    "broadcast_period_ms": 250
  },
  "vehicles": {
    "default_count": 10,
    "speed_kmh": 50
  },
  "waypoints": [
    {"name": "west_entry", "lat": -27.5919, "lon": -48.5482},
    {"name": "east_exit", "lat": -27.5919, "lon": -48.5382},
    {"name": "north_entry", "lat": -27.5869, "lon": -48.5432},
    {"name": "south_exit", "lat": -27.5969, "lon": -48.5432}
  ],
  "routes": [
    {"name": "west_to_east", "waypoints": ["west_entry", "east_exit"]},
    {"name": "east_to_west", "waypoints": ["east_exit", "west_entry"]},
    {"name": "north_to_south", "waypoints": ["north_entry", "south_exit"]},
    {"name": "south_to_north", "waypoints": ["south_exit", "north_entry"]}
  ]
}
```

## Usage

### Generate Trajectories
```bash
# Basic generation
python3 scripts/trajectory_generator_map_1.py

# Custom parameters
python3 scripts/trajectory_generator_map_1.py --vehicles 15 --duration 60

# Different config file
python3 scripts/trajectory_generator_map_1.py --config custom_config.json
```

### Run Demo
```bash
# Build and run
make demo
```

## Implementation Benefits

### ✅ **Simplicity**
- Single transmission radius for all entities
- Straight-line trajectories (no complex curves)
- Minimal configuration parameters

### ✅ **Predictability**  
- Vehicles always cross RSU collision domain
- Deterministic waypoint-based movement
- Clear geometric relationships

### ✅ **Maintainability**
- Fewer configuration parameters to manage
- Simple trajectory generation logic
- Easy to extend with additional waypoints

### ✅ **Performance**
- Minimal computational overhead
- Fast trajectory generation
- Efficient collision domain calculations

## Future Extensions

The simplified structure provides a solid foundation for future enhancements:

1. **Multi-RSU Maps**: Add more RSUs with overlapping coverage areas
2. **Multi-Waypoint Routes**: Extend routes to include multiple intermediate waypoints
3. **Traffic Patterns**: Implement directional vehicle flow patterns
4. **Dynamic Routing**: Add route selection based on traffic conditions

## File Structure

```
config/
  └── map_1_config.json          # Simplified configuration
scripts/
  └── trajectory_generator_map_1.py  # Waypoint-based generator
include/api/framework/
  └── map_config.h               # Simplified C++ config parser
tests/system_tests/
  └── demo.cpp                   # Updated demo using single radius
```

This simplified approach maintains the core collision domain functionality while reducing complexity and improving maintainability for future development.