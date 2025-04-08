# Vehicle Component System Changes

This document details the specific code changes implemented to enhance the vehicle system with component-based architecture, latency logging, and proper termination handling.

## New Files Created

### 1. `component.h`
- Created a base abstract `Component` class
- Implemented thread management (start/stop/join)
- Added CSV file handling for logging

### 2. `sender_component.h`
- Created a `SenderComponent` that inherits from `Component`
- Implemented message generation with microsecond-precision timestamps
- Added CSV logging of sent messages
- Format: `timestamp,source_vehicle,message_id,event_type`

### 3. `receiver_component.h`
- Created a `ReceiverComponent` that inherits from `Component`
- Added regex parsing to extract message metadata
- Implemented high-precision latency calculation (receive_time - send_time) in microseconds
- Added CSV logging with extended format: `receive_timestamp,source_vehicle,message_id,event_type,send_timestamp,latency_us`
- Added human-readable output of latency in both microseconds and milliseconds
- Incorporated bug fixes for EINTR signal handling from the previous implementation

## Modified Files

### 1. `vehicle.h` Changes
- Added component management with `std::vector<Component*> _components`
- Added methods:
  - `add_component(Component* component)`
  - `start_components()`
  - `stop_components()`
- Updated destructor to clean up components
- Modified `start()` and `stop()` to manage components
- Changed receive error return value from 0 to -1 for proper error handling in components
- Implemented robust shutdown mechanism:
  - Added close() call to communicator during stop()
  - Added running state checks before and after receive() calls
  - Improved error handling to distinguish between shutdown and real errors

### 2. `communicator.h` Changes
- Added close() method to gracefully terminate connections
- Improved handling of null buffers during shutdown vs. normal operation
- Added notify() mechanism to wake up threads blocked in receive()

### 3. `observer.h` Changes
- Added notify() method to allow waking up threads during shutdown
- Improved the semaphore handling for clean termination

### 4. `demo.cpp` Changes
- Replaced thread-based implementation with component instantiation
- Preserved signal handling for SIGUSR1 from bug fix
- Added filesystem handling to create logs directory
- Changed vehicle behavior:
  - Even ID vehicles: SenderComponent + ReceiverComponent
  - Odd ID vehicles: ReceiverComponent only
- Kept fixed vehicle lifetime (50s) from bug fix
- Ensured clean process termination through improved component shutdown

## Key Implementation Changes

### Message Format
Updated the message format to include microsecond-precision timestamp:
```
"Vehicle <id> message <counter> at <timestamp_us>"
```

### Component Lifecycle
1. Components are created during vehicle initialization
2. Components are started when vehicle starts
3. Component threads run independently
4. Components are stopped and joined when vehicle stops
5. Components are automatically deleted when vehicle is destroyed

### Latency Tracking
- Sender includes microsecond-precision timestamp in the message
- Receiver extracts timestamp with regex
- Latency calculated as (receive_timestamp_us - send_timestamp_us)
- Results displayed in both microseconds and milliseconds for better readability
- All data stored in CSV files for later analysis with high-precision values

### Clean Termination Process
The termination process follows these steps:
1. Vehicle lifetime expires
2. Vehicle::stop() is called
3. _running flag is set to false
4. _comms->close() is called to:
   - Detach from the channel
   - Signal any waiting threads with notify()
5. Components are stopped
6. Component threads are joined
7. Vehicle and components are destructed

## Thread Safety Improvements
- Added robust shutdown mechanism to prevent deadlocked threads
- Improved state checking to handle race conditions
- Added multiple check points for _running status
- Distinguished between expected shutdown errors and real errors

## Directory Structure
```
./logs/
  ├── vehicle_<id>.log         # Debug logs
  ├── vehicle_<id>_send.csv    # Sender timestamps
  └── vehicle_<id>_receive.csv # Receiver timestamps with latency
```

## Bug Fixes Preserved and Enhanced
- Signal handling (SIGUSR1) for properly terminating receive operations
- EINTR error handling in ReceiverComponent
- Proper handling of connection closure during shutdown
- Improved null buffer handling to distinguish shutdown vs. errors

The combined implementation now provides both the component-based architecture and robust termination handling, ensuring that all vehicles terminate cleanly after their lifetime expires. 