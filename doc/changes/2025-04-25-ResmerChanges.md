---
title: "Virtual Destination Address Implementation"
date: 2025-04-24
---

# Virtual Destination Address Test Modifications

## Overview
The new test now works partially as the message is delivered to the addressed component but the output that identifies the source and shows the message content is incorrect.
There are also problems within the termination procedures. Likely due to the changes made in the Component class.

## Changes

### Errors Fixed
**Compilation Errors fixed** Fixed errors in file tests/system_tests/virtual_dst_address_test.cpp
**Test runs correctly** Test starts and runs correctly but does not terminate properly as there are issues within the system's termination procedure likely because of the changes made in the components

### Port declaration changes
**Updated values for ports defined within the component headers** ECU1_PORT was initialized with value zero, but Vehicle expects the ports to start at 1.

### Before
`include/components/camera_component.h`
```cpp
// Assuming ECU1 will have port 0 based on creation order
const unsigned short ECU1_PORT = 0;
```
`include/components/lidar_component.h`
```cpp
// Assuming ECU2 will have port 1 based on creation order
const unsigned short ECU2_PORT = 1;
```
### After
`include/components/camera_component.h`
```cpp
// Assuming ECU1 will have port 1 based on creation order
const unsigned short ECU1_PORT = 1;
```
`include/components/lidar_component.h`
```cpp
// Assuming ECU2 will have port 2 based on creation order
const unsigned short ECU2_PORT = 2;
```