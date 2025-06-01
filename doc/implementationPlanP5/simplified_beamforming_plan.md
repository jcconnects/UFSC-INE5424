# Simplified Beamforming Implementation Plan

## Overview
Instead of modifying the NIC layer to understand Protocol internals, we implement beamforming filtering at the Protocol layer where it belongs. This maintains clean separation of concerns and requires minimal changes.

## Key Principles
1. **Keep NIC focused on Ethernet concerns**
2. **Add beamforming logic to Protocol::update() before notifying observers**
3. **Minimal changes to existing method signatures**
4. **Preserve layered architecture**

## Architectural Justification: Protocol-Level vs NIC-Level Filtering

### Why Protocol-Level Filtering is Superior

#### **Advantages of Protocol-Level Approach**

##### 1. **Protocol-Specific Isolation**
- **Scoped to Protocol 888**: Only affects our specific protocol packets, other protocols (HTTP, custom protocols, etc.) remain unaffected
- **No Cross-Protocol Interference**: NIC doesn't need to guess packet structures of different protocols
- **Safe Protocol Evolution**: We can change our packet structure without affecting NIC logic

##### 2. **Clean Architecture & Separation of Concerns**
```cpp
// Clean responsibility distribution:
// NIC Layer:      Ethernet frame handling, basic filtering by protocol number
// Protocol Layer: Application-specific packet parsing, beamforming logic  
// App Layer:      Business logic
```

##### 3. **Type Safety & Robustness**
```cpp
// Protocol layer knows the exact packet structure:
Packet* pkt = reinterpret_cast<Packet*>(buf->data()->payload);
BeamformingInfo* beam_info = pkt->beamforming(); // Safe cast

// vs NIC layer having to guess:
// What if protocol 889 has different structure? 
// What if BeamformingInfo is at different offset?
```

##### 4. **Maintainability & Extensibility**
- **Single Point of Truth**: Beamforming logic centralized in one place
- **Easy Testing**: Can unit test beamforming without setting up full network stack
- **Future-Proof**: Adding new beamforming features only requires Protocol changes

##### 5. **Backward Compatibility**
- Existing code using `Protocol::send(from, to, data, size)` continues working with omnidirectional defaults
- No breaking changes to lower layers

#### **Critical Issues with NIC-Level Filtering**

##### 1. **Protocol Structure Assumptions**
```cpp
// DANGEROUS: NIC assuming all protocols have BeamformingInfo
if (frame->prot == 888) {
    // What if we change our packet structure?
    // What if protocol 777 also wants beamforming but different format?
    BeamformingInfo* beam = reinterpret_cast<BeamformingInfo*>(
        frame->payload + FIXED_OFFSET  // Brittle!
    );
}
```

##### 2. **Hard-Coded Protocol Knowledge**
```cpp
// NIC would need to maintain protocol-specific logic:
switch(frame->prot) {
    case 888: /* Our beamforming logic */ break;
    case 777: /* Another protocol's structure? */ break;
    case 999: /* Yet another format? */ break;
    // This violates single responsibility principle
}
```

##### 3. **Maintenance Nightmare**
- Every protocol change requires NIC updates
- NIC becomes tightly coupled to all protocol implementations
- Testing becomes complex (need to test NIC with all possible protocols)

#### **Performance Analysis**

##### Protocol-Level Filtering Impact:
```
Packet Journey:
Network → SocketEngine (unavoidable) 
        → NIC::handle() (unavoidable, needed for basic Ethernet processing)
        → NIC::alloc() (small overhead, ~1µs)
        → Protocol::update() (HERE: beamforming filter, ~5-10µs for geo calculations)
        → Application (if packet passes)

Total "wasted" processing for dropped packets: ~6-11µs
```

##### Is This Acceptable?
- **Geo calculations are fast**: Haversine distance ~2µs, bearing ~3µs
- **Modern CPUs**: Can handle thousands of such calculations per second
- **Network bottleneck**: Physical network transmission (µs to ms) dominates
- **Early enough**: Filtered before reaching application logic where real work happens

#### **Real-World Considerations**

##### Multiple Protocol Scenario:
```cpp
// Your network might have:
Protocol 888: Your beamforming protocol
Protocol 889: Another team's protocol  
Protocol 777: Legacy protocol
Protocol 999: Future protocol with different beamforming

// NIC-level filtering would need to handle all these differently
// Protocol-level filtering: each protocol handles its own logic
```

##### Debugging & Monitoring:
```cpp
// Protocol-level filtering provides better visibility:
db<Protocol>(INF) << "[Protocol] Packet from " << src_mac 
                  << " dropped: out of range (" << distance << "m > " 
                  << beam_info->max_range << "m)\n";

// Clear logs about WHY packet was dropped, with protocol context
```

### **Disadvantages of Protocol-Level Filtering** (Minor)

1. **Later Filtering Point**: Packets go through `SocketEngine → NIC → Protocol` before being dropped
2. **Resource Usage**: Buffer allocation and timestamp processing happen before filtering
3. **Network Layer Processing**: Ethernet frame parsing still occurs

### **Conclusion**

**Protocol-level filtering is clearly superior** because:

1. **Correctness**: Only processes packets it understands (protocol 888)
2. **Safety**: No risk of misinterpreting other protocols' packet structures  
3. **Maintainability**: Changes isolated to relevant layer
4. **Performance**: Negligible overhead compared to network transmission
5. **Architecture**: Preserves clean layering principles

The small performance cost (6-11µs per filtered packet) is vastly outweighed by the architectural benefits and safety guarantees. In networking, correctness and maintainability trump micro-optimizations.

## Implementation Steps

### Step 1: Define Core Data Structures

Create `include/api/network/beamforming.h`:
```cpp
#ifndef BEAMFORMING_H
#define BEAMFORMING_H

#include <cstdint>

struct BeamformingInfo {
    double sender_latitude;
    double sender_longitude;
    float beam_center_angle;  // degrees, 0-359.9
    float beam_width_angle;   // degrees, 360.0 = omnidirectional
    float max_range;          // meters
    
    BeamformingInfo() : sender_latitude(0.0), sender_longitude(0.0),
                        beam_center_angle(0.0f), beam_width_angle(360.0f),
                        max_range(1000.0f) {}
} __attribute__((packed));

#endif // BEAMFORMING_H
```

### Step 2: Add Location Service

Create `include/api/framework/location_service.h`:
```cpp
#ifndef LOCATION_SERVICE_H
#define LOCATION_SERVICE_H

#include <mutex>

class LocationService {
public:
    static LocationService& getInstance() {
        static LocationService instance;
        return instance;
    }
    
    void getCurrentCoordinates(double& lat, double& lon) {
        std::lock_guard<std::mutex> lock(_mutex);
        lat = _latitude;
        lon = _longitude;
    }
    
    void setCurrentCoordinates(double lat, double lon) {
        std::lock_guard<std::mutex> lock(_mutex);
        _latitude = lat;
        _longitude = lon;
    }

private:
    LocationService() : _latitude(0.0), _longitude(0.0) {}
    double _latitude, _longitude;
    mutable std::mutex _mutex;
};

#endif // LOCATION_SERVICE_H
```

### Step 3: Add Geometry Utilities

Create `include/api/util/geo_utils.h`:
```cpp
#ifndef GEO_UTILS_H
#define GEO_UTILS_H

#include <cmath>

class GeoUtils {
public:
    static constexpr double EARTH_RADIUS_M = 6371000.0;
    
    static double haversineDistance(double lat1, double lon1, double lat2, double lon2) {
        double dlat = toRadians(lat2 - lat1);
        double dlon = toRadians(lon2 - lon1);
        double a = sin(dlat/2) * sin(dlat/2) + 
                   cos(toRadians(lat1)) * cos(toRadians(lat2)) * 
                   sin(dlon/2) * sin(dlon/2);
        return 2 * EARTH_RADIUS_M * asin(sqrt(a));
    }
    
    static float bearing(double lat1, double lon1, double lat2, double lon2) {
        double dlon = toRadians(lon2 - lon1);
        double y = sin(dlon) * cos(toRadians(lat2));
        double x = cos(toRadians(lat1)) * sin(toRadians(lat2)) - 
                   sin(toRadians(lat1)) * cos(toRadians(lat2)) * cos(dlon);
        return fmod(toDegrees(atan2(y, x)) + 360.0, 360.0);
    }
    
    static bool isInBeam(float bearing, float beam_center, float beam_width) {
        if (beam_width >= 360.0f) return true; // omnidirectional
        
        float half_width = beam_width / 2.0f;
        float diff = fmod(bearing - beam_center + 360.0f, 360.0f);
        return diff <= half_width || diff >= (360.0f - half_width);
    }

private:
    static double toRadians(double degrees) { return degrees * M_PI / 180.0; }
    static double toDegrees(double radians) { return radians * 180.0 / M_PI; }
};

#endif // GEO_UTILS_H
```

### Step 4: Modify Protocol::Packet Structure

Update `Protocol::Packet` in `protocol.h` to include `BeamformingInfo`:

```cpp
class Packet: public Header {
public:
    Packet() {}
    
    Header* header() { return this; }
    
    TimestampFields* timestamps() { 
        return reinterpret_cast<TimestampFields*>(
            reinterpret_cast<uint8_t*>(this) + sizeof(Header)
        ); 
    }
    
    BeamformingInfo* beamforming() {
        return reinterpret_cast<BeamformingInfo*>(
            reinterpret_cast<uint8_t*>(this) + sizeof(Header) + sizeof(TimestampFields)
        );
    }
    
    template<typename T>
    T* data() { 
        return reinterpret_cast<T*>(
            reinterpret_cast<uint8_t*>(this) + sizeof(Header) + sizeof(TimestampFields) + sizeof(BeamformingInfo)
        ); 
    }
} __attribute__((packed));

// Update MTU to account for BeamformingInfo
static const unsigned int MTU = NIC::MTU - sizeof(Header) - sizeof(TimestampFields) - sizeof(BeamformingInfo);
```

### Step 5: Add Beamforming to Protocol::send()

Add overloaded send method that accepts beamforming parameters:

```cpp
// Add to Protocol class
int send(Address from, Address to, const void* data, unsigned int size, const BeamformingInfo& beam_info) {
    // Similar to existing send(), but populate beam_info in packet
    // ... existing allocation and setup code ...
    
    // Set beamforming info
    *(packet->beamforming()) = beam_info;
    
    // ... rest of existing send logic ...
}

// Keep existing send() method for backward compatibility - uses default omnidirectional beam
int send(Address from, Address to, const void* data, unsigned int size) {
    BeamformingInfo default_beam; // defaults to omnidirectional
    return send(from, to, data, size, default_beam);
}
```

### Step 6: Add Beamforming Filter to Protocol::update()

This is the **key change** - add filtering before notifying observers:

```cpp
template <typename NIC>
void Protocol<NIC>::update(typename NIC::Protocol_Number prot, Buffer * buf) {
    db<Protocol>(TRC) << "Protocol<NIC>::update() called!\n";

    if (!buf) {
        db<Protocol>(INF) << "[Protocol] data received, but buffer is null. Releasing buffer.\n";
        return;
    }

    Packet* pkt = reinterpret_cast<Packet*>(buf->data()->payload);

    // **NEW: Beamforming check**
    BeamformingInfo* beam_info = pkt->beamforming();
    
    // Get receiver location
    double rx_lat, rx_lon;
    LocationService::getInstance().getCurrentCoordinates(rx_lat, rx_lon);
    
    // Check distance
    double distance = GeoUtils::haversineDistance(
        beam_info->sender_latitude, beam_info->sender_longitude,
        rx_lat, rx_lon
    );
    
    if (distance > beam_info->max_range) {
        db<Protocol>(INF) << "[Protocol] Packet dropped: out of range (" 
                          << distance << "m > " << beam_info->max_range << "m)\n";
        free(buf);
        return;
    }
    
    // Check beam angle (if not omnidirectional)
    if (beam_info->beam_width_angle < 360.0f) {
        float bearing = GeoUtils::bearing(
            beam_info->sender_latitude, beam_info->sender_longitude,
            rx_lat, rx_lon
        );
        
        if (!GeoUtils::isInBeam(bearing, beam_info->beam_center_angle, beam_info->beam_width_angle)) {
            db<Protocol>(INF) << "[Protocol] Packet dropped: outside beam (bearing=" 
                              << bearing << ", center=" << beam_info->beam_center_angle 
                              << ", width=" << beam_info->beam_width_angle << ")\n";
            free(buf);
            return;
        }
    }
    
    // **Continue with existing logic if packet passes beamforming checks**
    
    // Extract timestamps and update Clock if this is a PTP-relevant message
    TimestampFields* timestamps = pkt->timestamps();
    // ... rest of existing update() logic unchanged ...
}
```

### Step 7: Update Communicator (Optional)

Add convenience method to Communicator for beamforming sends:

```cpp
// Add to Communicator class
bool send(const Message_T* message, const BeamformingInfo& beam_info) {
    if (!_running.load(std::memory_order_acquire)) {
        return false;
    }
    
    int result = _channel->send(_address, Address::BROADCAST, 
                               message->data(), message->size(), beam_info);
    return result > 0;
}
```

## Benefits of This Approach

1. **Minimal Changes**: Only Protocol layer needs modification
2. **Clean Architecture**: Each layer maintains its responsibilities
3. **Backward Compatibility**: Existing code continues to work
4. **Early Filtering**: Packets are dropped at Protocol layer before reaching applications
5. **No Breaking Changes**: NIC and lower layers unchanged
6. **Simple Testing**: Easy to unit test beamforming logic in isolation

## Migration Path

1. Implement new structures and utilities
2. Add overloaded Protocol::send() method
3. Add filtering to Protocol::update()
4. Update applications to use beamforming parameters when needed
5. Test with existing code (should work unchanged with omnidirectional beams)

This approach is **much simpler** than the original plan while achieving the same functionality with better architecture. 