#ifndef VEHICLE_RSU_MANAGER_H
#define VEHICLE_RSU_MANAGER_H

#include <vector>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <memory>
#include <limits>

#include "api/framework/leaderKeyStorage.h"
#include "api/framework/clock.h"
#include "api/framework/location_service.h"
#include "api/util/geo_utils.h"
#include "api/util/debug.h"

// Forward declaration for Protocol
// template <typename NIC> class Protocol;

template <typename Protocol_T>
class VehicleRSUManager {
public:
    struct RSUInfo {
        typename Protocol_T::Address address;        // RSU network address
        double latitude;                             // RSU position
        double longitude;
        double radius;                               // RSU coverage radius
        MacKeyType group_key;                        // RSU authentication key
        std::chrono::steady_clock::time_point last_seen; // Last STATUS message time
        double distance_to_vehicle;                  // Calculated distance (updated dynamically)
        
        RSUInfo() : latitude(0), longitude(0), radius(0), distance_to_vehicle(std::numeric_limits<double>::max()) {}
        
        RSUInfo(const typename Protocol_T::Address& addr, double lat, double lon, 
                double r, const MacKeyType& key)
            : address(addr), latitude(lat), longitude(lon), radius(r), group_key(key),
              last_seen(std::chrono::steady_clock::now()), 
              distance_to_vehicle(std::numeric_limits<double>::max()) {}
    };

private:
    std::vector<RSUInfo> _known_rsus;           // List of known RSUs
    mutable std::mutex _rsu_list_mutex;         // Thread safety
    RSUInfo* _current_leader;                   // Current closest RSU
    std::chrono::seconds _rsu_timeout;          // RSU timeout period
    unsigned int _vehicle_id;                   // For logging

public:
    VehicleRSUManager(unsigned int vehicle_id, 
                     std::chrono::seconds timeout = std::chrono::seconds(10))
        : _current_leader(nullptr), _rsu_timeout(timeout), _vehicle_id(vehicle_id) {
        db<VehicleRSUManager>(INF) << "[Vehicle " << _vehicle_id 
                                   << "] RSU Manager initialized with " 
                                   << timeout.count() << "s timeout\n";
    }
    
    // Process incoming RSU STATUS message
    void process_rsu_status(const typename Protocol_T::Address& rsu_address,
                           double lat, double lon, double radius, 
                           const MacKeyType& group_key) {
        std::lock_guard<std::mutex> lock(_rsu_list_mutex);
        // Find existing RSU or create new one
        auto it = find_rsu_by_address(rsu_address);
        if (it != _known_rsus.end()) {
            // Update existing RSU
            it->latitude = lat;
            it->longitude = lon;
            it->radius = radius;
            it->group_key = group_key;
            it->last_seen = std::chrono::steady_clock::now();
            db<VehicleRSUManager>(INF) << "[Vehicle " << _vehicle_id 
                                       << "] Updated RSU " << rsu_address.to_string() << "\n";
        } else {
            // Add new RSU
            _known_rsus.emplace_back(rsu_address, lat, lon, radius, group_key);
            db<VehicleRSUManager>(INF) << "[Vehicle " << _vehicle_id 
                                       << "] Discovered new RSU " << rsu_address.to_string() 
                                       << " at (" << lat << ", " << lon << ")\n";
        }
        // Trigger leader selection update
        update_leader_selection();
    }
    
    // Update distances and leader selection
    void update_leader_selection() {
        // Must be called with _rsu_list_mutex held
        if (_known_rsus.empty()) {
            _current_leader = nullptr;
            db<VehicleRSUManager>(INF) << "[Vehicle " << _vehicle_id 
                                       << "] No RSUs available for leader selection\n";
            return;
        }
        // Update distances to all RSUs
        update_distances();
        // Sort RSUs by distance (closest first)
        std::sort(_known_rsus.begin(), _known_rsus.end(),
                  [](const RSUInfo& a, const RSUInfo& b) {
                      return a.distance_to_vehicle < b.distance_to_vehicle;
                  });
        // Select closest RSU that's within range
        RSUInfo* new_leader = nullptr;
        for (auto& rsu : _known_rsus) {
            if (rsu.distance_to_vehicle <= rsu.radius) {
                new_leader = &rsu;
                break;
            }
        }
        // Check if leader changed
        bool leader_changed = (_current_leader != new_leader);
        _current_leader = new_leader;
        if (leader_changed) {
            if (_current_leader) {
                db<VehicleRSUManager>(INF) << "[Vehicle " << _vehicle_id 
                                           << "] New leader selected: " << _current_leader->address.to_string()
                                           << " (distance: " << _current_leader->distance_to_vehicle << "m)\n";
                // Update global leader storage
                LeaderKeyStorage::getInstance().setLeaderId(_current_leader->address.paddr());
                LeaderKeyStorage::getInstance().setGroupMacKey(_current_leader->group_key);
                // Update Clock for PTP synchronization
                LeaderIdType leader_id = static_cast<LeaderIdType>(_current_leader->address.paddr().bytes[5]);
                Clock::getInstance().setSelfId(leader_id);
                Clock::getInstance().activate(nullptr);
            } else {
                db<VehicleRSUManager>(WRN) << "[Vehicle " << _vehicle_id 
                                           << "] No RSU in range - no leader selected\n";
            }
        }
    }
    
    // Calculate distances from vehicle to all RSUs
    void update_distances() {
        // Get current vehicle position
        double vehicle_lat, vehicle_lon;
        LocationService::getCurrentCoordinates(vehicle_lat, vehicle_lon);
        // Calculate distance to each RSU
        for (auto& rsu : _known_rsus) {
            rsu.distance_to_vehicle = GeoUtils::haversineDistance(
                vehicle_lat, vehicle_lon, rsu.latitude, rsu.longitude);
        }
    }
    
private:
    // Find RSU by address
    typename std::vector<RSUInfo>::iterator find_rsu_by_address(
        const typename Protocol_T::Address& address) {
        return std::find_if(_known_rsus.begin(), _known_rsus.end(),
            [&address](const RSUInfo& info) {
                return info.address == address;
            });
    }
};

#endif // VEHICLE_RSU_MANAGER_H 