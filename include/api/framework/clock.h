#ifndef CLOCK_H
#define CLOCK_H

#include <chrono>
#include <atomic>
#include <mutex>
#include <cmath>    // For std::abs, std::sqrt
#include <limits>   // For std::numeric_limits
#include "api/util/debug.h"  // For db<> logging
#include "api/traits.h"      // For traits specialization
#include "api/framework/leaderKeyStorage.h"  // For LeaderKeyStorage
#include "api/network/ethernet.h"  // For Ethernet::Address

// --- Placeholder types (These should be defined in appropriate common headers) ---
// Represents a timestamp, using milliseconds since a steady epoch
using TimestampType = std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>;

// Represents a duration, e.g., for offsets
using DurationType = std::chrono::milliseconds;

// Represents frequency error, unitless (e.g., parts per million or direct ratio)
using FrequencyErrorType = double;

// Identifier for a PTP leader (e.g., a vehicle ID or MAC address representation)
// For simplicity, using uint32_t. Adapt as needed.
using LeaderIdType = uint32_t;
const LeaderIdType INVALID_LEADER_ID = 0; // Example invalid ID

// Structure to pass PTP-relevant data (extracted from message headers) to the Clock's activate method
struct PtpRelevantData {
    LeaderIdType sender_id;         // ID of the message sender (potential PTP master)
    TimestampType ts_tx_at_sender;  // Timestamp from the sender's NIC/PTP header (ts_PN equivalent)
    TimestampType ts_local_rx;      // Local hardware timestamp when this node's NIC received the frame
};

// Structure to store processed PTP message data internally by the Clock
struct PtpInternalMessageInfo {
    TimestampType ts_tx_at_sender = TimestampType::min();   // ts_PN equivalent
    TimestampType ts_local_rx = TimestampType::min();    // ts_N_prime (local hardware time of reception)
    DurationType d_tx_calc = DurationType::zero();         // Calculated total propagation delay
    TimestampType leader_time_at_local_rx_event = TimestampType::min(); // ts_PN + d_TX_calc

    PtpInternalMessageInfo() = default;
};
// --- End Placeholder types ---


/**
 * @brief Thread-safe singleton for PTP clock synchronization
 * 
 * This class is thread-safe and can be accessed from multiple threads.
 * All state access is protected by mutex locks.
 * The singleton instance is initialized in a thread-safe manner using C++11's
 * static initialization guarantees.
 */
class Clock {
public:
    enum class State {
        UNSYNCHRONIZED,
        AWAITING_SECOND_MSG, // Offset acquired, awaiting second message for drift
        SYNCHRONIZED         // Offset and drift acquired
    };

    // Deleted copy constructor and assignment operator
    Clock(const Clock&) = delete;
    Clock& operator=(const Clock&) = delete;

    static Clock& getInstance();
    void setSelfId(LeaderIdType id);

    // --- Core Public Interface ---

    void activate(const PtpRelevantData* new_msg_data);
    TimestampType getSynchronizedTime(bool* is_synchronized);
    bool isFullySynchronized() const;
    State getState();
    LeaderIdType getCurrentLeader() const;
    TimestampType getLocalSteadyHardwareTime() const;
    TimestampType getLocalSystemTime();
    DurationType getMaxLeaderSilenceInterval() const;
    void reset();

private:
    // Private constructor for singleton
    Clock() :
        _currentState(State::UNSYNCHRONIZED),
        _current_offset(DurationType::zero()),
        _current_drift_fe(0.0),
        _leader_time_at_last_sync_event(TimestampType::min()),
        _local_time_at_last_sync_event(TimestampType::min()),
        _current_leader_id(INVALID_LEADER_ID),
        _leader_has_changed_flag(false),
        _self_id(INVALID_LEADER_ID) {
        doClearSyncData();
        db<Clock>(INF) << "Clock: Initialized in UNSYNCHRONIZED state\n";
    }
    ~Clock() = default; // Default destructor

    // --- State Machine Actions (called with _mutex held) ---
    void doClearSyncData();
    void doProcessFirstLeaderMsg(const PtpRelevantData& msg_data);
    void doProcessSecondLeaderMsgAndCalcDrift(const PtpRelevantData& msg_data);
    void doProcessSubsequentLeaderMsg(const PtpRelevantData& msg_data);


    // --- Guard Conditions ---
    bool isLeaderMessageTimedOut() const;
    bool isMessageFromCurrentLeader(const PtpRelevantData& msg_data) const;
    bool isLeaderAssigned() const;
    bool checkAndHandleLeaderChange(LeaderIdType storage_leader_id);

    // --- Member Variables ---
    std::atomic<State> _currentState;  // Made atomic for better performance
    mutable std::mutex _mutex;

    // PTP Synchronization Data
    PtpInternalMessageInfo _msg1_data; // Data from the (N-1)th PTP-relevant message
    PtpInternalMessageInfo _msg2_data; // Data from the (N)th (latest) PTP-relevant message

    DurationType _current_offset;         // O = LocalRxTime - (LeaderTxTimeAtSender + PropagationDelay)
    FrequencyErrorType _current_drift_fe; // Frequency error (drift rate) relative to leader

    TimestampType _leader_time_at_last_sync_event; // Leader's clock value (ts_tx_at_sender + d_tx_calc) at the moment of the last local RX event used for sync
    TimestampType _local_time_at_last_sync_event;  // Local hardware clock value at that same local RX event

    LeaderIdType _current_leader_id;
    std::atomic<bool> _leader_has_changed_flag;  // Made atomic for better performance

    LeaderIdType _self_id;

    // Configuration Constants
    // Allow up to 10ms cumulative error:
    // For standard crystal specification worst-case (~20 ppb): 10ms / 20ppb = 500ms
    static constexpr DurationType MAX_LEADER_SILENCE_INTERVAL = std::chrono::milliseconds(500);
};

/**
 * @brief Get the singleton instance
 * @return Reference to the singleton instance
 * 
 * Thread-safe: Uses C++11's static initialization guarantees
 */
inline Clock& Clock::getInstance() {
    static Clock instance;
    return instance;
}

/**
 * @brief Set the self ID for this clock instance (node's own PTP-relevant ID)
 * @param id The LeaderIdType of this node
 *
 * Thread-safe: Protected by mutex
 */
inline void Clock::setSelfId(LeaderIdType id) {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_self_id == INVALID_LEADER_ID && id != INVALID_LEADER_ID) {
        _self_id = id;
        db<Clock>(INF) << "Clock: Self ID set to " << _self_id << "\n";
    } else if (id != INVALID_LEADER_ID && _self_id != id && _self_id != INVALID_LEADER_ID) {
        // If already set and trying to change to a different valid ID
        db<Clock>(WRN) << "Clock: Attempt to change self ID from " << _self_id 
                       << " to " << id << ". Current self ID maintained.\n";
    } else if (id == _self_id) {
        // Setting to the same ID, no action needed but maybe log for debugging
        db<Clock>(TRC) << "Clock: Self ID re-confirmed to " << _self_id << "\n";
    }
}

/**
 * @brief Activate the state machine with new PTP data
 * @param new_msg_data PTP data from incoming message or nullptr for timeout checks
 * 
 * Thread-safe: Protected by mutex
 */
inline void Clock::activate(const PtpRelevantData* new_msg_data) {
    std::lock_guard<std::mutex> lock(_mutex);

    // Get current leader from storage without holding its mutex
    LeaderIdType storage_leader_id = INVALID_LEADER_ID;
    {
        auto& storage = LeaderKeyStorage::getInstance();
        Ethernet::Address storage_leader = storage.getLeaderId();
        storage_leader_id = storage_leader.bytes[5];
    }

    // NEW: Check if this node IS the leader (according to storage)
    if (_self_id != INVALID_LEADER_ID && _self_id == storage_leader_id) {
        if (_currentState.load(std::memory_order_acquire) != State::SYNCHRONIZED || _current_leader_id != _self_id) {
            db<Clock>(INF) << "Clock: This node (" << _self_id << ") is the PTP leader. Forcing SYNCHRONIZED state.\n";
            _currentState.store(State::SYNCHRONIZED, std::memory_order_release);
            _current_leader_id = _self_id; // This node is the leader
            _current_offset = DurationType::zero();
            _current_drift_fe = 0.0;
            _local_time_at_last_sync_event = getLocalSteadyHardwareTime();
            _leader_time_at_last_sync_event = _local_time_at_last_sync_event; // Leader time is its own local time
            _msg1_data = PtpInternalMessageInfo(); // Clear old sync message data
            _msg2_data = PtpInternalMessageInfo();
            _leader_has_changed_flag.store(false, std::memory_order_release); 
        }
        // As a leader, its clock is authoritative. No further processing of PTP messages for sync.
        return;
    }

    // If this node is NOT the leader, proceed with normal PTP logic.
    // Check and handle leader change (original logic, might set state to UNSYNCHRONIZED)
    [[maybe_unused]] bool actual_leader_change_occurred = checkAndHandleLeaderChange(storage_leader_id);

    State current_state_local = _currentState.load(std::memory_order_acquire);
    State new_state = current_state_local;

    // Main state transitions
    switch (current_state_local) {
        case State::UNSYNCHRONIZED:
            if (new_msg_data && isLeaderAssigned() && isMessageFromCurrentLeader(*new_msg_data)) {
                new_state = State::AWAITING_SECOND_MSG;
                doProcessFirstLeaderMsg(*new_msg_data);
            }
            break;

        case State::AWAITING_SECOND_MSG:
            if (isLeaderMessageTimedOut()) {
                new_state = State::UNSYNCHRONIZED;
                doClearSyncData();
            } else if (new_msg_data && isMessageFromCurrentLeader(*new_msg_data)) {
                new_state = State::SYNCHRONIZED;
                doProcessSecondLeaderMsgAndCalcDrift(*new_msg_data);
            }
            break;

        case State::SYNCHRONIZED:
            if (isLeaderMessageTimedOut()) {
                new_state = State::UNSYNCHRONIZED;
                doClearSyncData();
            } else if (new_msg_data && isMessageFromCurrentLeader(*new_msg_data)) {
                doProcessSubsequentLeaderMsg(*new_msg_data);
            }
            break;
    }

    // Log state transition if it changed
    if (new_state != current_state_local) {
        db<Clock>(INF) << "Clock: " << current_state_local << " -> " << new_state << "\n";
        _currentState.store(new_state, std::memory_order_release);
    }
}

/**
 * @brief Get the current synchronized time
 * @return Current PTP-synchronized timestamp
 * 
 * Thread-safe: Protected by mutex
 */
inline TimestampType Clock::getSynchronizedTime(bool* is_synchronized) {
    std::lock_guard<std::mutex> lock(_mutex);
    TimestampType local_hw_now = getLocalSteadyHardwareTime();
    State current_state_local = _currentState.load(std::memory_order_acquire);

    if (current_state_local == State::UNSYNCHRONIZED) {
        db<Clock>(INF) << "Clock::getSynchronizedTime WARNING: Clock UNSYNCHRONIZED. Returning local hardware time.\n";
        *is_synchronized = false;
        return local_hw_now;
    }

    DurationType elapsed_since_last_sync = local_hw_now - _local_time_at_last_sync_event;

    if (current_state_local == State::AWAITING_SECOND_MSG) {
        // Only offset correction: SynchronizedTime = LeaderTimeAtSync + ElapsedLocal
        // Note: _current_offset = _local_time_at_last_sync_event - _leader_time_at_last_sync_event
        // So, LeaderTimeAtSync = _local_time_at_last_sync_event - _current_offset
        // SynchronizedTime = (_local_time_at_last_sync_event - _current_offset) + elapsed_since_last_sync
        // SynchronizedTime = local_hw_now - _current_offset
        *is_synchronized = false;
        return local_hw_now - _current_offset;
    }

    // State::SYNCHRONIZED: Apply offset and drift correction
    // SynchronizedTime = local_hw_now - (_current_offset_at_last_event + drift_correction_for_elapsed_time)
    // Or, using the state machine's variables:
    // leader_increment = elapsed_local * (1.0 - current_drift_fe)
    // sync_time = leader_time_at_last_sync_event + leader_increment
    double leader_increment_double = static_cast<double>(elapsed_since_last_sync.count()) * (1.0 - _current_drift_fe);
    DurationType leader_increment = std::chrono::milliseconds(static_cast<long long>(leader_increment_double));
    *is_synchronized = true;
    return _leader_time_at_last_sync_event + leader_increment;
}

/**
 * @brief Check if clock is fully synchronized
 * @return true if in SYNCHRONIZED state
 * 
 * Thread-safe: Protected by mutex
 */
inline bool Clock::isFullySynchronized() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _currentState.load(std::memory_order_acquire) == State::SYNCHRONIZED;
}

/**
 * @brief Get current synchronization state
 * @return Current state
 * 
 * Thread-safe: Protected by mutex
 */
inline Clock::State Clock::getState() {
    std::lock_guard<std::mutex> lock(_mutex);

    State current_state_local = _currentState.load(std::memory_order_acquire);
    
    // NEW: If this node is the leader, it's always synchronized.
    if (_self_id != INVALID_LEADER_ID && _current_leader_id == _self_id) {
        if (current_state_local != State::SYNCHRONIZED) {
             // This ensures that if somehow the state was different, it's corrected.
            db<Clock>(INF) << "Clock::getState: This node (" << _self_id << ") is leader. Correcting state to SYNCHRONIZED.\n";
            _currentState.store(State::SYNCHRONIZED, std::memory_order_release);
        }
        return State::SYNCHRONIZED;
    }
    
    // Only check timeout for states that expect leader messages (non-leader case)
    if (current_state_local == State::AWAITING_SECOND_MSG || 
        current_state_local == State::SYNCHRONIZED) {
        
        if (isLeaderMessageTimedOut()) {
            db<Clock>(INF) << "Clock: Timeout detected, transitioning to UNSYNCHRONIZED\n";
            _currentState.store(State::UNSYNCHRONIZED, std::memory_order_release);
            doClearSyncData();
            return State::UNSYNCHRONIZED;
        }
    }
    return current_state_local;
}

/**
 * @brief Get the current PTP leader ID
 * @return Current leader ID
 * 
 * Thread-safe: Protected by mutex
 */
inline LeaderIdType Clock::getCurrentLeader() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _current_leader_id;
}

// Provides the local hardware time (monotonic steady clock)
inline TimestampType Clock::getLocalSteadyHardwareTime() const {
    return std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now());
}

// Provides the local system time (wall clock, can change)
inline TimestampType Clock::getLocalSystemTime() {
    // Since we want to maintain a single timestamp type (steady_clock),
    // we'll just return the steady clock time
    // This is a simplification - in a real system you might want to
    // maintain the relationship between system and steady time
    return getLocalSteadyHardwareTime();
}

inline DurationType Clock::getMaxLeaderSilenceInterval() const {
    return MAX_LEADER_SILENCE_INTERVAL;
}

/**
 * @brief Reset the Clock singleton to its initial state (for testing only)
 *
 * This method is NOT thread-safe and should only be used in test setup/teardown.
 */
inline void Clock::reset() {
    std::lock_guard<std::mutex> lock(_mutex);
    _currentState.store(State::UNSYNCHRONIZED, std::memory_order_release);
    _current_leader_id = INVALID_LEADER_ID;
    doClearSyncData();
    _self_id = INVALID_LEADER_ID; // Reset self_id as well for testing
}

inline void Clock::doClearSyncData() {
    _msg1_data = PtpInternalMessageInfo();
    _msg2_data = PtpInternalMessageInfo();
    _current_offset = DurationType::zero();
    _current_drift_fe = 0.0;
    _leader_time_at_last_sync_event = TimestampType::min();
    _local_time_at_last_sync_event = getLocalSteadyHardwareTime();
    _leader_has_changed_flag.store(false, std::memory_order_release);
    db<Clock>(INF) << "Clock: Sync data cleared\n";
}

void Clock::doProcessFirstLeaderMsg(const PtpRelevantData& msg_data) {
    // Action for: UNSYNCHRONIZED --> AWAITING_SECOND_MSG
    _msg1_data.ts_tx_at_sender = msg_data.ts_tx_at_sender;
    _msg1_data.ts_local_rx = msg_data.ts_local_rx;

    _msg1_data.d_tx_calc = std::chrono::milliseconds(2);

    _msg1_data.leader_time_at_local_rx_event = _msg1_data.ts_tx_at_sender + _msg1_data.d_tx_calc;
    _current_offset = _msg1_data.ts_local_rx - _msg1_data.leader_time_at_local_rx_event;

    _leader_time_at_last_sync_event = _msg1_data.leader_time_at_local_rx_event;
    _local_time_at_last_sync_event = _msg1_data.ts_local_rx;
    _current_drift_fe = 0.0;
    
    db<Clock>(INF) << "Clock: Processed first leader message. Offset: " 
        << _current_offset.count() << "ms\n";
}

void Clock::doProcessSecondLeaderMsgAndCalcDrift(const PtpRelevantData& msg_data) {
    // Action for: AWAITING_SECOND_MSG --> SYNCHRONIZED
    _msg2_data.ts_tx_at_sender = msg_data.ts_tx_at_sender;
    _msg2_data.ts_local_rx = msg_data.ts_local_rx;

    _msg2_data.d_tx_calc = std::chrono::milliseconds(2);
    _msg2_data.leader_time_at_local_rx_event = _msg2_data.ts_tx_at_sender + _msg2_data.d_tx_calc;

    DurationType o1 = _msg1_data.ts_local_rx - _msg1_data.leader_time_at_local_rx_event;
    DurationType o2 = _msg2_data.ts_local_rx - _msg2_data.leader_time_at_local_rx_event;
    _current_offset = o2; 

    DurationType delta_o = o2 - o1;
    // Use effective leader timestamps at local reception events for delta_T
    DurationType delta_t_leader_effective = _msg2_data.leader_time_at_local_rx_event - _msg1_data.leader_time_at_local_rx_event;

    if (delta_t_leader_effective.count() > 0) {
        _current_drift_fe = static_cast<FrequencyErrorType>(delta_o.count()) / static_cast<FrequencyErrorType>(delta_t_leader_effective.count());
    } else {
        // std::cerr << "Clock WARNING: delta_T_leader_effective is zero or negative during drift calculation. Drift not updated from " << _current_drift_fe << ".\n";
    }
    
    _leader_time_at_last_sync_event = _msg2_data.leader_time_at_local_rx_event;
    _local_time_at_last_sync_event = _msg2_data.ts_local_rx;
    
    db<Clock>(INF) << "Clock: Processed second leader message. New Offset: " 
        << _current_offset.count() << "ms, Drift FE: " << _current_drift_fe << "\n";
}

void Clock::doProcessSubsequentLeaderMsg(const PtpRelevantData& msg_data) {
    // Action for: SYNCHRONIZED --> SYNCHRONIZED
    _msg1_data = _msg2_data;
    doProcessSecondLeaderMsgAndCalcDrift(msg_data);
    
    db<Clock>(INF) << "Clock: Processed subsequent leader message. Updated Offset: " 
        << _current_offset.count() << "ms, Updated Drift FE: " << _current_drift_fe << "\n";
}

bool Clock::isLeaderMessageTimedOut() const {
    // NEW: If this node is the current leader, it doesn't time out on itself.
    // Note: _current_leader_id and _self_id are read. Callers must hold _mutex.
    if (_self_id != INVALID_LEADER_ID && _current_leader_id == _self_id) {
        return false;
    }

    if (_local_time_at_last_sync_event == TimestampType::min() || !isLeaderAssigned()) {
            // No sync event yet or no leader, so not "timed out" in the sense of expecting a message
        return false;
    }
    TimestampType local_hw_now = getLocalSteadyHardwareTime();
    return (local_hw_now - _local_time_at_last_sync_event) > MAX_LEADER_SILENCE_INTERVAL;
}

inline bool Clock::isMessageFromCurrentLeader(const PtpRelevantData& msg_data) const {
    return msg_data.sender_id == _current_leader_id;
}

inline bool Clock::isLeaderAssigned() const {
    return _current_leader_id != INVALID_LEADER_ID;
}

/**
 * @brief Check for leader change and handle state transition if needed
 * @param storage_leader_id The current leader ID from LeaderKeyStorage
 * @return true if leader changed and state was reset, false if no change occurred
 * 
 * This method compares the current leader ID with the one from storage.
 * If they differ, it updates the current leader, forces a transition to 
 * UNSYNCHRONIZED state, and clears all synchronization data.
 * This ensures the clock restarts synchronization with the new leader.
 * 
 * Called with _mutex held.
 */
bool Clock::checkAndHandleLeaderChange(LeaderIdType storage_leader_id) {
    if (_current_leader_id != storage_leader_id) {
        db<Clock>(INF) << "Clock: Leader changed from " << _current_leader_id 
            << " to " << storage_leader_id << " during activation\n";
        _current_leader_id = storage_leader_id;
        _currentState.store(State::UNSYNCHRONIZED, std::memory_order_release);
        doClearSyncData();
        return true; // Leader changed
    }
    return false; // No change
}

// Stream operator for Clock::State
inline std::ostream& operator<<(std::ostream& os, Clock::State state) {
    switch (state) {
        case Clock::State::UNSYNCHRONIZED:
            return os << "UNSYNCHRONIZED";
        case Clock::State::AWAITING_SECOND_MSG:
            return os << "AWAITING_SECOND_MSG";
        case Clock::State::SYNCHRONIZED:
            return os << "SYNCHRONIZED";
        default:
            return os << "UNKNOWN_STATE";
    }
}

#endif // CLOCK_H