#ifndef LEADER_KEY_STORAGE_H
#define LEADER_KEY_STORAGE_H

#include <mutex>
#include <array>
#include <atomic>
#include <chrono>
#include "api/network/ethernet.h"
#include "api/util/debug.h"
#include "api/traits.h"

// Define MAC key type
using MacKeyType = std::array<uint8_t, 16>;

/**
 * @brief Thread-safe singleton for leader key storage
 * 
 * This class is thread-safe and can be accessed from multiple threads.
 * All public methods are protected by mutex locks.
 * The singleton instance is initialized in a thread-safe manner using C++11's
 * static initialization guarantees.
 */
class LeaderKeyStorage {
public:
    static LeaderKeyStorage& getInstance();
    void setLeaderId(const Ethernet::Address& leader_id);
    Ethernet::Address getLeaderId() const;
    void setGroupMacKey(const MacKeyType& key);
    MacKeyType getGroupMacKey() const;
    std::chrono::steady_clock::time_point getLastUpdateTime() const;

private:
    LeaderKeyStorage();
    ~LeaderKeyStorage() = default;
    LeaderKeyStorage(const LeaderKeyStorage&) = delete;
    LeaderKeyStorage& operator=(const LeaderKeyStorage&) = delete;

    mutable std::mutex _mutex;
    Ethernet::Address _current_leader_id;
    MacKeyType _current_group_mac_key;
    std::atomic<std::chrono::steady_clock::time_point> _last_update_time;
};

// Implementation

/**
 * @brief Get the singleton instance
 * @return Reference to the singleton instance
 * 
 * Thread-safe: Uses C++11's static initialization guarantees
 */
inline LeaderKeyStorage& LeaderKeyStorage::getInstance() {
    static LeaderKeyStorage instance;
    return instance;
}

/**
 * @brief Set the current leader ID
 * @param leader_id The MAC address of the new leader
 * 
 * Thread-safe: Protected by mutex
 */
inline void LeaderKeyStorage::setLeaderId(const Ethernet::Address& leader_id) {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_current_leader_id != leader_id) {
        db<LeaderKeyStorage>(INF) << "LeaderKeyStorage: Leader changed from " 
            << Ethernet::mac_to_string(_current_leader_id) << " to " 
            << Ethernet::mac_to_string(leader_id) << "\n";
        _current_leader_id = leader_id;
        _last_update_time.store(std::chrono::steady_clock::now(), std::memory_order_release);
    }
}

/**
 * @brief Get the current leader ID
 * @return The MAC address of the current leader
 * 
 * Thread-safe: Protected by mutex
 */
inline Ethernet::Address LeaderKeyStorage::getLeaderId() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _current_leader_id;
}

/**
 * @brief Set the current group MAC key
 * @param key The new MAC key
 * 
 * Thread-safe: Protected by mutex
 */
inline void LeaderKeyStorage::setGroupMacKey(const MacKeyType& key) {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_current_group_mac_key != key) {
        db<LeaderKeyStorage>(INF) << "LeaderKeyStorage: Group MAC key updated\n";
        _current_group_mac_key = key;
        _last_update_time.store(std::chrono::steady_clock::now(), std::memory_order_release);
    }
}

/**
 * @brief Get the current group MAC key
 * @return The current MAC key
 * 
 * Thread-safe: Protected by mutex
 */
inline MacKeyType LeaderKeyStorage::getGroupMacKey() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _current_group_mac_key;
}

/**
 * @brief Get the last update time
 * @return The time of the last update
 * 
 * Thread-safe: Uses atomic operations
 */
inline std::chrono::steady_clock::time_point LeaderKeyStorage::getLastUpdateTime() const {
    return _last_update_time.load(std::memory_order_acquire);
}

inline LeaderKeyStorage::LeaderKeyStorage() : 
    _current_leader_id(Ethernet::NULL_ADDRESS),
    _last_update_time(std::chrono::steady_clock::now()) { 
    _current_group_mac_key.fill(0);
    db<LeaderKeyStorage>(INF) << "LeaderKeyStorage: Initialized\n";
}

#endif // LEADER_KEY_STORAGE_H 