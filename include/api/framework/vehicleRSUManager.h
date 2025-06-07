#ifndef VEHICLE_RSU_MANAGER_H
#define VEHICLE_RSU_MANAGER_H

#include <vector>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <memory>
#include <limits>
#include <cstdio> // For snprintf in debug logging

#include "api/framework/leaderKeyStorage.h"
#include "api/framework/clock.h"
#include "api/framework/location_service.h"
#include "api/util/geo_utils.h"
#include "api/util/debug.h"
#include "api/framework/periodicThread.h"

// Forward declaration for Protocol
// template <typename NIC> class Protocol;

/**
 * VehicleRSUManager - Simplified collision domain logic
 * 
 * Leader selection is based on communication ability (collision domain).
 * If a vehicle can receive STATUS messages from an RSU, they are within
 * the collision domain and the RSU is eligible to be the leader.
 * 
 * The closest RSU (by distance) that we can communicate with becomes the leader.
 */
template <typename Protocol_T>
class VehicleRSUManager {
public:
    struct RSUInfo {
        typename Protocol_T::Address address;        // RSU network address
        double latitude;                             // RSU position
        double longitude;
        double radius;                               // RSU coverage radius (for info only)
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
    
    // Neighbor RSU keys (just keys, not full info)
    std::vector<MacKeyType> _neighbor_rsu_keys;
    mutable std::mutex _neighbor_keys_mutex;    // Thread safety for neighbor keys
    
    // Periodic cleanup thread
    std::unique_ptr<Periodic_Thread<VehicleRSUManager>> _cleanup_thread;
    std::atomic<bool> _running;

public:
    VehicleRSUManager(unsigned int vehicle_id, 
                     std::chrono::seconds timeout = std::chrono::seconds(10))
        : _current_leader(nullptr), _rsu_timeout(timeout), _vehicle_id(vehicle_id), _running(true) {
        db<VehicleRSUManager>(INF) << "[RSUManager " << _vehicle_id 
                                   << "] RSU Manager initialized with " 
                                   << timeout.count() << "s timeout\n";
        
        // Start periodic cleanup thread (cleanup every 5 seconds)
        db<VehicleRSUManager>(TRC) << "[RSUManager " << _vehicle_id 
                                   << "] Starting periodic cleanup thread (5s interval)\n";
        _cleanup_thread = std::make_unique<Periodic_Thread<VehicleRSUManager>>(
            this, &VehicleRSUManager::cleanup_stale_rsus
        );
        _cleanup_thread->start(5000000); // 5 seconds in microseconds
        db<VehicleRSUManager>(TRC) << "[RSUManager " << _vehicle_id 
                                   << "] Periodic cleanup thread started\n";
    }
    
    ~VehicleRSUManager() {
        db<VehicleRSUManager>(TRC) << "[RSUManager " << _vehicle_id 
                                   << "] RSU Manager shutting down\n";
        _running.store(false, std::memory_order_release);
        if (_cleanup_thread) {
            db<VehicleRSUManager>(TRC) << "[RSUManager " << _vehicle_id 
                                       << "] Stopping periodic cleanup thread\n";
            _cleanup_thread->join();
        }
        db<VehicleRSUManager>(INF) << "[RSUManager " << _vehicle_id << "] RSU Manager destroyed\n";
    }
    
    // Process incoming RSU STATUS message
    void process_rsu_status(const typename Protocol_T::Address& rsu_address,
                           double lat, double lon, double radius, 
                           const MacKeyType& group_key) {
        if (!_running.load(std::memory_order_acquire)) return;
        
        std::lock_guard<std::mutex> lock(_rsu_list_mutex);
        
        db<VehicleRSUManager>(TRC) << "[RSUManager " << _vehicle_id 
                                   << "] Processing RSU STATUS from " << rsu_address.to_string()
                                   << " at (" << lat << ", " << lon << ") radius=" << radius << "m\n";
        
        // Debug logging: Show RSU key
        std::string rsu_key_hex = "";
        for (size_t i = 0; i < 16; ++i) {
            char hex_byte[4];
            snprintf(hex_byte, sizeof(hex_byte), "%02X ", group_key[i]);
            rsu_key_hex += hex_byte;
        }
        db<VehicleRSUManager>(INF) << "[RSUManager " << _vehicle_id 
                                   << "] RSU key received: " << rsu_key_hex << "\n";
        
        // Check if this key exists in neighbor keys and remove it (we now have full info)
        remove_neighbor_rsu_key(group_key);
        
        // Find existing RSU or create new one
        auto it = find_rsu_by_address(rsu_address);
        if (it != _known_rsus.end()) {
            // Update existing RSU
            it->latitude = lat;
            it->longitude = lon;
            it->radius = radius;
            it->group_key = group_key;
            it->last_seen = std::chrono::steady_clock::now();
            db<VehicleRSUManager>(INF) << "[RSUManager " << _vehicle_id 
                                       << "] Updated RSU " << rsu_address.to_string() << "\n";
        } else {
            // Add new RSU
            _known_rsus.emplace_back(rsu_address, lat, lon, radius, group_key);
            db<VehicleRSUManager>(INF) << "[RSUManager " << _vehicle_id 
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
            if (_current_leader != nullptr) {
                db<VehicleRSUManager>(INF) << "[RSUManager " << _vehicle_id 
                                           << "] Lost all RSUs - clearing leader\n";
            }
            _current_leader = nullptr;
            db<VehicleRSUManager>(TRC) << "[RSUManager " << _vehicle_id 
                                       << "] No RSUs available for leader selection\n";
            return;
        }
        
        db<VehicleRSUManager>(TRC) << "[RSUManager " << _vehicle_id 
                                   << "] Updating leader selection among " << _known_rsus.size() << " RSUs\n";
        
        // Update distances to all RSUs
        update_distances();
        
        // Sort RSUs by distance (closest first)
        std::sort(_known_rsus.begin(), _known_rsus.end(),
                  [](const RSUInfo& a, const RSUInfo& b) {
                      return a.distance_to_vehicle < b.distance_to_vehicle;
                  });
        
        // Log all RSU distances for debugging
        for (size_t i = 0; i < _known_rsus.size(); ++i) {
            const auto& rsu = _known_rsus[i];
            db<VehicleRSUManager>(TRC) << "[RSUManager " << _vehicle_id 
                                       << "] RSU " << rsu.address.to_string()
                                       << " distance=" << rsu.distance_to_vehicle << "m\n";
        }
        
        // Select closest RSU (we can communicate with all RSUs in our list)
        // If we received STATUS messages from them, they are within collision domain
        RSUInfo* new_leader = &_known_rsus[0]; // Closest RSU
        
        // Check if leader changed
        bool leader_changed = (_current_leader != new_leader);
        
        if (leader_changed) {
            if (_current_leader && new_leader) {
                db<VehicleRSUManager>(INF) << "[RSUManager " << _vehicle_id 
                                           << "] Leader changed from " << _current_leader->address.to_string()
                                           << " to " << new_leader->address.to_string() << "\n";
            } else if (!_current_leader && new_leader) {
                db<VehicleRSUManager>(INF) << "[RSUManager " << _vehicle_id 
                                           << "] First leader selected: " << new_leader->address.to_string() << "\n";
            }
        }
        
        _current_leader = new_leader;
        
        if (_current_leader) {
            db<VehicleRSUManager>(INF) << "[RSUManager " << _vehicle_id 
                                       << "] Current leader: " << _current_leader->address.to_string()
                                       << " (distance: " << _current_leader->distance_to_vehicle << "m)\n";
            
            if (leader_changed) {
                // Update global leader storage
                LeaderIdType leader_id = static_cast<LeaderIdType>(_current_leader->address.paddr().bytes[5]);
                db<VehicleRSUManager>(TRC) << "[RSUManager " << _vehicle_id 
                                           << "] Updating global leader storage with ID " << (int)leader_id << "\n";
                
                LeaderKeyStorage::getInstance().setLeaderId(_current_leader->address.paddr());
                LeaderKeyStorage::getInstance().setGroupMacKey(_current_leader->group_key);
                
                // Update Clock for PTP synchronization
                Clock::getInstance().setSelfId(leader_id);
                Clock::getInstance().activate(nullptr);
            }
        }
    }
    
    // Calculate distances from vehicle to all RSUs
    void update_distances() {
        // Get current vehicle position
        double vehicle_lat, vehicle_lon;
        LocationService::getCurrentCoordinates(vehicle_lat, vehicle_lon);
        
        db<VehicleRSUManager>(TRC) << "[RSUManager " << _vehicle_id 
                                   << "] Vehicle position: (" << vehicle_lat << ", " << vehicle_lon << ")\n";
        
        // Calculate distance to each RSU
        for (auto& rsu : _known_rsus) {
            rsu.distance_to_vehicle = GeoUtils::haversineDistance(
                vehicle_lat, vehicle_lon, rsu.latitude, rsu.longitude);
        }
    }
    
    // Cleanup stale RSUs (called by periodic thread)
    void cleanup_stale_rsus() {
        if (!_running.load(std::memory_order_acquire)) return;
        
        std::lock_guard<std::mutex> lock(_rsu_list_mutex);
        auto now = std::chrono::steady_clock::now();
        bool list_changed = false;
        
        db<VehicleRSUManager>(TRC) << "[RSUManager " << _vehicle_id 
                                   << "] Running periodic RSU cleanup, checking " << _known_rsus.size() << " RSUs\n";
        
        auto it = _known_rsus.begin();
        while (it != _known_rsus.end()) {
            auto time_since_last_seen = std::chrono::duration_cast<std::chrono::seconds>(now - it->last_seen);
            
            if (time_since_last_seen > _rsu_timeout) {
                db<VehicleRSUManager>(INF) << "[RSUManager " << _vehicle_id 
                                           << "] Removing stale RSU " << it->address.to_string() 
                                           << " (last seen " << time_since_last_seen.count() << "s ago)\n";
                
                // Check if we're removing the current leader
                if (_current_leader == &(*it)) {
                    db<VehicleRSUManager>(WRN) << "[RSUManager " << _vehicle_id 
                                               << "] Removing current leader due to timeout\n";
                    _current_leader = nullptr;
                    list_changed = true;
                }
                
                it = _known_rsus.erase(it);
                list_changed = true;
            } else {
                db<VehicleRSUManager>(TRC) << "[RSUManager " << _vehicle_id 
                                           << "] RSU " << it->address.to_string() 
                                           << " is fresh (last seen " << time_since_last_seen.count() << "s ago)\n";
                ++it;
            }
        }
        
        if (list_changed) {
            db<VehicleRSUManager>(INF) << "[RSUManager " << _vehicle_id 
                                       << "] RSU list changed after cleanup, updating leader selection\n";
            update_leader_selection();
        } else {
            db<VehicleRSUManager>(TRC) << "[RSUManager " << _vehicle_id 
                                       << "] No changes after RSU cleanup\n";
        }
    }
    
    // Get current leader info (for debugging)
    RSUInfo* get_current_leader() {
        std::lock_guard<std::mutex> lock(_rsu_list_mutex);
        return _current_leader;
    }
    
    // Get all known RSUs (for debugging)
    std::vector<RSUInfo> get_known_rsus() {
        std::lock_guard<std::mutex> lock(_rsu_list_mutex);
        return _known_rsus; // Copy for thread safety
    }
    
    // Add neighbor RSU key
    void add_neighbor_rsu_key(const MacKeyType& key) {
        std::lock_guard<std::mutex> lock(_neighbor_keys_mutex);
        // Check if key already exists
        for (const auto& existing_key : _neighbor_rsu_keys) {
            if (existing_key == key) {
                db<VehicleRSUManager>(INF) << "[RSUManager " << _vehicle_id 
                                           << "] Neighbor RSU key already exists\n";
                return;
            }
        }
        _neighbor_rsu_keys.push_back(key);
        db<VehicleRSUManager>(INF) << "[RSUManager " << _vehicle_id 
                                   << "] Added neighbor RSU key (total: " << _neighbor_rsu_keys.size() << ")\n";
    }
    
    // Get all neighbor RSU keys (for MAC verification)
    std::vector<MacKeyType> get_neighbor_rsu_keys() {
        std::lock_guard<std::mutex> lock(_neighbor_keys_mutex);
        return _neighbor_rsu_keys; // Copy for thread safety
    }
    
    // Remove neighbor RSU key (when we get full RSU info via STATUS)
    bool remove_neighbor_rsu_key(const MacKeyType& key) {
        std::lock_guard<std::mutex> lock(_neighbor_keys_mutex);
        auto it = std::find(_neighbor_rsu_keys.begin(), _neighbor_rsu_keys.end(), key);
        if (it != _neighbor_rsu_keys.end()) {
            _neighbor_rsu_keys.erase(it);
            db<VehicleRSUManager>(INF) << "[RSUManager " << _vehicle_id 
                                       << "] Removed neighbor RSU key (remaining: " << _neighbor_rsu_keys.size() << ")\n";
            return true;
        }
        return false;
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