# Vehicle Component System Changes

This document details the specific code changes implemented to enhance the vehicle system with component-based architecture and latency logging.

## New Files Created

### 1. `component.h`
- Created a base abstract `Component` class
- Implemented thread management (start/stop/join)
- Added CSV file handling for logging

### 2. `sender_component.h`
- Created a `SenderComponent` that inherits from `Component`
- Implemented message generation with timestamps
- Added CSV logging of sent messages
- Format: `timestamp,source_vehicle,message_id,event_type`

### 3. `receiver_component.h`
- Created a `ReceiverComponent` that inherits from `Component`
- Added regex parsing to extract message metadata
- Implemented latency calculation (receive_time - send_time)
- Added CSV logging with extended format: `receive_timestamp,source_vehicle,message_id,event_type,send_timestamp,latency_ms`

## Modified Files

### 1. `vehicle.h` Changes
- Added component management with `std::vector<Component*> _components`
- Added methods:
  - `add_component(Component* component)`
  - `start_components()`
  - `stop_components()`
- Updated destructor to clean up components
- Modified `start()` and `stop()` to manage components

### 2. `demo.cpp` Changes
- Removed thread-based implementation (`send_thread`, `receive_thread`)
- Replaced with component instantiation based on vehicle ID
- Added filesystem handling to create logs directory
- Updated vehicle cleanup
- Changed vehicle behavior:
  - Even ID vehicles: SenderComponent + ReceiverComponent
  - Odd ID vehicles: ReceiverComponent only

## Key Implementation Changes

### Message Format
Updated the message format to include timestamp information:
```
"Vehicle <id> message <counter> at <timestamp>"
```

### Component Lifecycle
1. Components are created during vehicle initialization
2. Components are started when vehicle starts
3. Component threads run independently
4. Components are stopped and joined when vehicle stops
5. Components are automatically deleted when vehicle is destroyed

### Latency Tracking
- Sender stores timestamp in the message itself
- Receiver extracts timestamp with regex
- Latency = receive_timestamp - send_timestamp
- All data stored in CSV files for later analysis

## Directory Structure
```
./logs/
  ├── vehicle_<id>.log         # Debug logs
  ├── vehicle_<id>_send.csv    # Sender timestamps
  └── vehicle_<id>_receive.csv # Receiver timestamps with latency
```
