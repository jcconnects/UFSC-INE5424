# Autonomous Vehicle Communication System - Use Cases

*Date: 2025-04-28*  
*Author: João Pedro Schmidt Cordeiro*

This document describes the key use cases for our autonomous vehicle communication system, highlighting how the communication framework components interact in various scenarios.

## Core Use Cases

### 1. Vehicle-to-Vehicle Collision Avoidance

**Primary Components:**
- Camera Component: Provides visual detection data (cars, pedestrians, etc.)
- LiDAR Component: Generates point cloud data for precise distance measurements
- Perception Component: Subscribes to camera/lidar data, processes fused sensor data
- Planning Component: Subscribes to perception data, identifies collision risks
- Control Component: Subscribes to planning commands, executes avoidance maneuvers

**Communication Flow:**
1. Camera/LiDAR components broadcast detection data
2. Perception component receives this data (Observer pattern)
3. Perception component broadcasts fused object tracks
4. Planning component receives tracks, detects collision risks
5. Planning component broadcasts avoidance commands
6. Control component receives and executes these commands

**Communication Features Demonstrated:**
- Observer pattern for asynchronous event propagation
- Broadcast communication for data sharing

### 2. Cooperative Intersection Management

**Primary Components:**
- INS Component: Provides position/velocity data
- V2X Component: Subscribes to INS data, handles cross-vehicle negotiation
- Planning Component: Subscribes to V2X intersection schedule, generates trajectories
- Control Component: Subscribes to planning trajectories, executes maneuvers

**Communication Flow:**
1. INS Component broadcasts position using the API
2. V2X Component observes this data, broadcasts intersection intent
3. V2X Components from different vehicles exchange intersection schedules
4. Planning Component subscribes to intersection schedule
5. Planning Component publishes trajectory to follow schedule
6. Control Component subscribes to and executes this trajectory

**Communication Features Demonstrated:**
- Time-triggered publish-subscribe for periodic updates
- Group communication for multi-vehicle coordination

### 3. Energy-Efficient Route Planning

**Primary Components:**
- Battery Component: Provides energy status information
- INS Component: Provides location data
- V2X Component: Subscribes to INS data, exchanges traffic information
- Planning Component: Subscribes to battery status and V2X data, optimizes routes

**Communication Flow:**
1. Battery Component provides state of charge
2. Planning Component considers energy constraints
3. V2X Component obtains traffic/road condition data from other vehicles
4. Planning Module calculates energy-optimal route

**Communication Features Demonstrated:**
- Temporal synchronization via PTP for consistent timestamps
- Shared data management and history

### 4. Sensor Data Sharing and Fusion

**Primary Components:**
- Sensor Components (Camera, LiDAR, INS): Provide raw sensor data
- V2X Component: Subscribes to processed sensor data, exchanges with other vehicles
- Perception Component: Subscribes to local sensors and V2X data, integrates all sources

**Communication Flow:**
1. V2X Component broadcasts/receives sensor data summaries
2. V2X Components synchronize time using PTP
3. Perception Component integrates local and external sensor data
4. Perception Component extends environmental model beyond line-of-sight

**Communication Features Demonstrated:**
- Secure group communication with message authentication
- Temporal data synchronization across vehicles

## Additional Use Cases

### 5. Emergency Vehicle Response

**Primary Components:**
- V2X Component: Receives emergency notifications
- Safety Component: Subscribes to V2X emergency messages, validates authenticity
- Planning Component: Subscribes to safety validations, calculates yielding trajectories
- Control Component: Subscribes to planning commands, executes maneuvers

**Communication Flow:**
1. V2X Component receives emergency vehicle approach notification
2. Safety Component validates authenticity of emergency message
3. Planning Component calculates safe yielding trajectory
4. Control Component executes appropriate maneuver

### 6. Cooperative Perception at Blind Intersections

**Primary Components:**
- Camera/LiDAR Components: Provide limited sensor data
- V2X Component: Subscribes to local perception limitations, requests external data
- Perception Component: Subscribes to both local sensors and V2X external data
- Safety Component: Subscribes to enhanced perception output, verifies safety

**Communication Flow:**
1. V2X Component detects vehicles at blind intersection
2. V2X Component requests/receives sensor data from better-positioned vehicles
3. Perception Component incorporates external data
4. Safety Component verifies safety before proceeding
5. Planning and Control execute appropriate action

### 7. Battery Health Monitoring and Charging Recommendations

**Primary Components:**
- Battery Component: Provides detailed status
- INS Component: Provides location data
- V2X Component: Subscribes to battery status and location, exchanges charging data
- Planning Component: Subscribes to V2X charging information, adapts route planning

**Communication Flow:**
1. Battery Component detects suboptimal performance
2. V2X Component requests charging station availability/wait times
3. Planning Component integrates charging stop into route
4. V2X Component reserves charging slot

### 8. Adaptive Cruise Control with V2V Enhancement

**Primary Components:**
- INS Component: Provides velocity/position data
- Camera/LiDAR Components: Monitor vehicles ahead
- V2X Component: Subscribes to local vehicle data, exchanges with vehicles ahead
- Control Component: Subscribes to both direct sensing and V2X data, adjusts controls

**Communication Flow:**
1. Control Component maintains normal ACC operation
2. V2X Component receives acceleration data from vehicles ahead in traffic
3. Control Component adapts earlier to traffic flow changes
4. V2X Component communicates own acceleration changes to vehicles behind

## Communication Patterns Demonstrated

Across these use cases, our implementation demonstrates:

1. **Observer Pattern**: Asynchronous event propagation between components
2. **Time-Triggered Publish-Subscribe**: Periodic data updates without explicit requests
3. **Temporal Synchronization**: PTP-based timestamp consistency across vehicles
4. **Secure Group Communication**: Message authentication and integrity verification
5. **Shared Memory Communication**: Efficient intra-vehicle component communication
6. **Broadcast Communication**: Reaching multiple receivers efficiently
7. **Mobile Group Management**: Dynamic group formation and dissolution as vehicles move
