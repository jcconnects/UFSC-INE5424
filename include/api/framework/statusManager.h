#ifndef STATUS_MANAGER_H
#define STATUS_MANAGER_H

#include <map>
#include <mutex>
#include <chrono>
#include <array>
#include <atomic>
#include <vector>
#include <algorithm> // For std::max_element
#include <stdexcept> // For std::invalid_argument

// Assuming these paths are correct relative to your include directories
#include "api/network/protocol.h"
#include "api/framework/periodicThread.h" // Your Periodic_Thread
#include "api/util/debug.h"        // Your debug logging
#include "api/framework/leaderKeyStorage.h"

// Forward declaration of Protocol to manage include order if necessary
template <typename NIC_TYPE> class Protocol;

// Define a type for the unique key of a vehicle
using UniqueKeyValueType = std::array<uint8_t, 16>; // Example: 128-bit key

/**
 * @brief Thread-safe manager for vehicle status and leader election
 * 
 * This class is thread-safe and can be accessed from multiple threads.
 * It manages vehicle status broadcasts, neighbor tracking, and leader election.
 * All state access is protected by appropriate synchronization primitives.
 */
template <typename NIC_TYPE>
class StatusManager {
public:
    using VehicleIdType = typename Protocol<NIC_TYPE>::Address; // MAC + Port
    // Define the port used for STATUS messages
    // This should ideally come from a central traits/configuration file.
    typename Protocol<NIC_TYPE>::Port STATUS_PORT = 60000;

    struct NeighborInfo {
        VehicleIdType id;
        std::atomic<uint32_t> age;  // Made atomic for potential concurrent updates
        UniqueKeyValueType unique_key;
        std::chrono::steady_clock::time_point last_seen;
    };

    /**
     * @brief Construct a new Status Manager
     * 
     * Thread-safe: Constructor is not thread-safe, should be called from a single thread
     */
    StatusManager(
        Protocol<NIC_TYPE>* owner_protocol,
        const Ethernet::Address& self_mac_address,
        uint32_t self_age,
        const UniqueKeyValueType& self_unique_key,
        std::chrono::microseconds broadcast_interval = std::chrono::seconds(1),
        std::chrono::microseconds prune_interval = std::chrono::seconds(3),
        std::chrono::microseconds neighbor_timeout = std::chrono::seconds(5)
    ) : _protocol_ptr(owner_protocol),
        _self_mac_address(self_mac_address),
        _self_id(self_mac_address, STATUS_PORT),
        _self_age(self_age),
        _self_unique_key(self_unique_key),
        _broadcast_interval(broadcast_interval),
        _prune_interval(prune_interval),
        _neighbor_timeout(neighbor_timeout),
        _running(true)
    {
        if (!_protocol_ptr) {
            db<StatusManager>(ERR) << "StatusManager: Owner protocol cannot be null\n";
            throw std::invalid_argument("StatusManager: Owner protocol cannot be null");
        }

        // Add self to neighbor list
        {
            std::lock_guard<std::mutex> lock(_neighbor_list_mutex);
            _neighbor_list[_self_id] = {_self_id, self_age, _self_unique_key, std::chrono::steady_clock::now()};
        }
        
        // Set self as initial leader
        LeaderKeyStorage::getInstance().setLeaderId(_self_mac_address);
        LeaderKeyStorage::getInstance().setGroupMacKey(_self_unique_key);

        _broadcast_thread = new Periodic_Thread<StatusManager>(this, &StatusManager::broadcast_status_message_task);
        _prune_thread = new Periodic_Thread<StatusManager>(this, &StatusManager::prune_stale_neighbors_task);

        _broadcast_thread->start(_broadcast_interval);
        _prune_thread->start(_prune_interval);

        db<StatusManager>(INF) << "StatusManager initialized for " << _self_id.to_string() 
            << " (Age: " << _self_age << "). Broadcasting every " 
            << _broadcast_interval.count() << "us. Pruning every " 
            << _prune_interval.count() << "us.\n";
    }

    /**
     * @brief Destroy the Status Manager
     * 
     * Thread-safe: Destructor is not thread-safe, should be called from a single thread
     */
    ~StatusManager() {
        _running.store(false, std::memory_order_release);
        if (_broadcast_thread) {
            _broadcast_thread->join();
            delete _broadcast_thread;
            _broadcast_thread = nullptr;
        }
        if (_prune_thread) {
            _prune_thread->join();
            delete _prune_thread;
            _prune_thread = nullptr;
        }
        db<StatusManager>(INF) << "StatusManager for " << _self_id.to_string() << " shut down.\n";
    }

    /**
     * @brief Process an incoming status message
     * 
     * Thread-safe: Protected by mutex for neighbor list access
     */
    void process_incoming_status(
        const VehicleIdType& sender_protocol_address,
        const uint8_t* payload_data,
        unsigned int payload_size)
    {
        if (!_running.load(std::memory_order_acquire)) return;

        if (payload_size < (sizeof(uint32_t) + sizeof(UniqueKeyValueType))) {
            db<StatusManager>(WAR) << "StatusManager: Received undersized STATUS payload from "
                << sender_protocol_address.to_string() << ". Size: " << payload_size << "\n";
            return;
        }

        uint32_t sender_age;
        UniqueKeyValueType sender_unique_key;

        unsigned int offset = 0;
        std::memcpy(&sender_age, payload_data + offset, sizeof(sender_age));
        offset += sizeof(sender_age);
        std::memcpy(sender_unique_key.data(), payload_data + offset, sender_unique_key.size());

        VehicleIdType consistent_sender_id(sender_protocol_address.paddr(), STATUS_PORT);

        std::lock_guard<std::mutex> lock(_neighbor_list_mutex);

        auto it = _neighbor_list.find(consistent_sender_id);
        bool list_changed = false;

        if (it == _neighbor_list.end()) {
            _neighbor_list[consistent_sender_id] = {consistent_sender_id, sender_age, sender_unique_key, std::chrono::steady_clock::now()};
            list_changed = true;
            db<StatusManager>(INF) << "StatusManager: New neighbor " << consistent_sender_id.to_string()
                << " (Age: " << sender_age << ").\n";
        } else {
            if (it->second.age != sender_age || it->second.unique_key != sender_unique_key) {
                list_changed = true;
                db<StatusManager>(INF) << "StatusManager: Updated neighbor " << consistent_sender_id.to_string()
                    << " (Age: " << sender_age << ").\n";
            }
            it->second.age.store(sender_age, std::memory_order_release);
            it->second.unique_key = sender_unique_key;
            it->second.last_seen = std::chrono::steady_clock::now();
        }

        if (list_changed) {
            perform_leader_election_and_update_storage_unsafe();
        }
    }

    StatusManager(const StatusManager&) = delete;
    StatusManager& operator=(const StatusManager&) = delete;

private:
    /**
     * @brief Broadcast status message task
     * 
     * Thread-safe: Uses atomic running flag and protocol mutex
     */
    void broadcast_status_message_task() {
        if (!_running.load(std::memory_order_acquire)) return;

        std::vector<uint8_t> payload;
        payload.resize(sizeof(_self_age) + _self_unique_key.size());

        unsigned int offset = 0;
        std::memcpy(payload.data() + offset, &_self_age, sizeof(_self_age));
        offset += sizeof(_self_age);
        std::memcpy(payload.data() + offset, _self_unique_key.data(), _self_unique_key.size());
        
        typename Protocol<NIC_TYPE>::Address broadcast_dest_addr(Ethernet::BROADCAST, STATUS_PORT);
        
        {
            std::lock_guard<std::mutex> lock(_protocol_mutex);
            _protocol_ptr->send(_self_id, broadcast_dest_addr, payload.data(), payload.size());
        }
        
        db<StatusManager>(TRC) << "StatusManager: Broadcasted STATUS from " << _self_id.to_string() 
            << " (Age: " << _self_age << ").\n";
    }

    /**
     * @brief Prune stale neighbors task
     * 
     * Thread-safe: Protected by mutex for neighbor list access
     */
    void prune_stale_neighbors_task() {
        if (!_running.load(std::memory_order_acquire)) return;

        std::lock_guard<std::mutex> lock(_neighbor_list_mutex);
        bool list_changed = false;
        auto now = std::chrono::steady_clock::now();

        for (auto it = _neighbor_list.begin(); it != _neighbor_list.end(); ) {
            if (it->first.paddr() == _self_mac_address) {
                it->second.last_seen = now;
                ++it;
                continue;
            }
            if ((now - it->second.last_seen) > _neighbor_timeout) {
                db<StatusManager>(INF) << "StatusManager: Pruning stale neighbor " << it->first.to_string() << ".\n";
                it = _neighbor_list.erase(it);
                list_changed = true;
            } else {
                ++it;
            }
        }

        if (list_changed) {
            perform_leader_election_and_update_storage_unsafe();
        }
    }

    /**
     * @brief Perform leader election and update storage
     * 
     * Thread-safe: Must be called with _neighbor_list_mutex held
     */
    void perform_leader_election_and_update_storage_unsafe() {
        if (_neighbor_list.empty()) {
            db<StatusManager>(WAR) << "StatusManager: Neighbor list became empty. Re-asserting self as leader.\n";
            LeaderKeyStorage::getInstance().setLeaderId(_self_mac_address);
            LeaderKeyStorage::getInstance().setGroupMacKey(_self_unique_key);
            return;
        }

        auto leader_it = std::max_element(_neighbor_list.begin(), _neighbor_list.end(),
            [](const auto& a_pair, const auto& b_pair) {
                const NeighborInfo& a = a_pair.second;
                const NeighborInfo& b = b_pair.second;
                if (a.age.load(std::memory_order_acquire) != b.age.load(std::memory_order_acquire)) {
                    return a.age.load(std::memory_order_acquire) < b.age.load(std::memory_order_acquire);
                }
                return b.unique_key < a.unique_key;
            });

        Ethernet::Address elected_leader_mac = leader_it->second.id.paddr();
        const UniqueKeyValueType& elected_leader_key = leader_it->second.unique_key;

        db<StatusManager>(INF) << "StatusManager: Leader election completed. New leader MAC: "
            << Ethernet::mac_to_string(elected_leader_mac) 
            << " (Age: " << leader_it->second.age.load(std::memory_order_acquire) << ").\n";

        LeaderKeyStorage::getInstance().setLeaderId(elected_leader_mac);
        LeaderKeyStorage::getInstance().setGroupMacKey(elected_leader_key);
    }

    Protocol<NIC_TYPE>* _protocol_ptr;
    mutable std::mutex _protocol_mutex;  // Protect protocol access
    const Ethernet::Address _self_mac_address;
    const VehicleIdType _self_id;
    std::atomic<uint32_t> _self_age;  // Made atomic for potential concurrent updates
    const UniqueKeyValueType _self_unique_key;

    std::map<VehicleIdType, NeighborInfo> _neighbor_list;
    mutable std::mutex _neighbor_list_mutex;

    Periodic_Thread<StatusManager>* _broadcast_thread;
    Periodic_Thread<StatusManager>* _prune_thread;

    const std::chrono::microseconds _broadcast_interval;
    const std::chrono::microseconds _prune_interval;
    const std::chrono::microseconds _neighbor_timeout;

    std::atomic<bool> _running;
};

// Add traits for StatusManager
template<typename NIC_TYPE>
struct Traits<StatusManager<NIC_TYPE>> : public Traits<void> {
    static const bool debugged = true;
};

#endif // STATUS_MANAGER_H