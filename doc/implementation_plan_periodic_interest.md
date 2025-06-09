# Implementation Plan: Periodic Interest Messages

## Overview

This document outlines the implementation plan for converting the current single-shot INTEREST message system to a periodic system, as specified in the Time-Triggered Publish-Subscribe requirements.

### Current System State
- Consumers send **one** INTEREST message at startup (hardcoded 1-second period)
- Producers respond to INTEREST messages using basic GCD period calculation
- No interest lifetime management or renewal mechanism

### Target System State
- Consumers send INTEREST messages **periodically** with configurable intervals
- Producers track multiple consumer periods and calculate proper GCD
- Interest messages have lifetime management and expiration handling
- Full compliance with TEDS (IEEE 1451) specification

---

## Implementation Phases

### Phase 1: Consumer-Side Periodic Interest Generation 🔥 **HIGH PRIORITY**

**Goal**: Modify consumers to send INTEREST messages periodically instead of just once.

#### 1.1 Extend Agent Class Structure

**Files to Modify:**
- `include/api/framework/agent.h`

**New Member Variables:**
```cpp
// For consumers (who send periodic INTEREST)
Thread* _interest_thread;              // Periodic thread for sending INTEREST
Microseconds _requested_period;        // Period requested by this consumer
std::atomic<bool> _interest_active;    // Control interest sending
std::atomic<bool> _is_consumer;        // Track if this agent is a consumer
```

**New Method Declarations:**
```cpp
/**
 * @brief Start sending periodic INTEREST messages for the specified unit and period
 * 
 * @param unit The data type unit to request
 * @param period The desired response period from producers
 * @return int Success/failure status
 */
int start_periodic_interest(Unit unit, Microseconds period);

/**
 * @brief Stop sending periodic INTEREST messages
 */
void stop_periodic_interest();

/**
 * @brief Send a single INTEREST message (called by periodic thread)
 * 
 * @param unit The data type unit
 */
void send_interest(Unit unit);

/**
 * @brief Update the period for periodic interest sending
 * 
 * @param new_period The new period to use
 */
void update_interest_period(Microseconds new_period);
```

#### 1.2 Modify Agent Constructor

**Current Code Location:** Lines 67-84 in `agent.h`

**Changes Needed:**
```cpp
// Replace this block:
if (type == Type::RESPONSE) {
    Microseconds period(1000000);
    send(unit, period); // Send initial INTEREST
}

// With this:
_is_consumer = (type == Type::RESPONSE);
_interest_active = false;
_interest_thread = nullptr;
_requested_period = Microseconds::zero();

if (_is_consumer) {
    // Don't send initial INTEREST here anymore
    // Application will call start_periodic_interest() when ready
}
```

#### 1.3 Implement Periodic Interest Methods

**Implementation Location:** After line 263 in `agent.h`

```cpp
inline int Agent::start_periodic_interest(Unit unit, Microseconds period) {
    if (!_is_consumer) {
        db<Agent>(WRN) << "[Agent] " << _name << " is not a consumer, cannot start periodic interest\n";
        return -1;
    }
    
    if (_interest_active.load()) {
        db<Agent>(INF) << "[Agent] " << _name << " updating interest period from " 
                       << _requested_period.count() << " to " << period.count() << " microseconds\n";
        update_interest_period(period);
        return 0;
    }
    
    _requested_period = period;
    _interest_active = true;
    
    if (!_interest_thread) {
        _interest_thread = new Thread(this, &Agent::send_interest, unit);
        _interest_thread->start(period.count());
        db<Agent>(INF) << "[Agent] " << _name << " started periodic INTEREST for unit: " 
                       << unit << " with period: " << period.count() << " microseconds\n";
    }
    
    return 0;
}

inline void Agent::stop_periodic_interest() {
    if (_interest_active.load()) {
        _interest_active.store(false);
        
        if (_interest_thread) {
            _interest_thread->join();
            delete _interest_thread;
            _interest_thread = nullptr;
        }
        
        db<Agent>(INF) << "[Agent] " << _name << " stopped periodic INTEREST\n";
    }
}

inline void Agent::send_interest(Unit unit) {
    if (!_interest_active.load() || !running()) {
        return;
    }
    
    db<Agent>(TRC) << "[Agent] " << _name << " sending periodic INTEREST for unit: " 
                   << unit << " with period: " << _requested_period.count() << " microseconds\n";
    
    Message msg(Message::Type::INTEREST, _address, unit, _requested_period);
    log_message(msg, "SEND");
    _can->send(&msg);
}

inline void Agent::update_interest_period(Microseconds new_period) {
    _requested_period = new_period;
    if (_interest_thread) {
        _interest_thread->adjust_period(new_period.count());
    }
}
```

#### 1.4 Update Agent Destructor

**Location:** Lines 85-110 in `agent.h`

**Add before periodic thread cleanup:**
```cpp
// Stop interest thread if it exists
if (_interest_thread) {
    stop_periodic_interest();
}
```

#### 1.5 Update Consumer Applications

**Files to Modify:**
- `include/app/components/basic_consumer_a.h`
- `include/app/components/basic_consumer_b.h`

**Changes:** Add method to start periodic interest:
```cpp
void start_consuming(Microseconds period = Microseconds(1000000)) {
    start_periodic_interest(static_cast<std::uint32_t>(DataTypes::UNIT_A), period);
}
```

#### 1.6 Testing Phase 1

**Test Files:**
- `tests/unit_tests/agent_test.cpp` - Comprehensive Agent testing

**Test Organization:**
The `agent_test.cpp` file provides comprehensive testing of all Agent functionality, organized into these categories:

1. **Basic Agent Functionality Tests**
   - Agent construction and initialization
   - Constructor parameter validation
   - Destructor cleanup verification
   - Basic send/receive operations
   - Message handling

2. **Periodic Interest Functionality Tests (Phase 1)**
   - `start_periodic_interest()` method testing
   - Consumer validation (producers should be rejected)
   - Period update functionality
   - `stop_periodic_interest()` method testing
   - Idempotent stop operations
   - `send_interest()` safety checks
   - `update_interest_period()` functionality
   - Thread creation and management
   - State management validation

3. **Integration Tests**
   - Consumer-producer interaction
   - Multiple consumers with single producer
   - End-to-end message flow verification

4. **Thread Safety Tests**
   - Concurrent periodic interest operations
   - Multi-threaded Agent operations

5. **Edge Cases and Error Conditions**
   - Very short/long periods
   - Zero periods
   - Invalid state operations

**Test Implementation Approach:**
- Based on existing test patterns (`clock_test.cpp`, etc.)
- Uses TestCase base class with setUp/tearDown methods
- Includes comprehensive Doxygen documentation
- Provides helper methods for test setup and validation
- Implements a TestAgent class for controlled testing

---

### Phase 2: Producer-Side Enhanced GCD Calculation 🔥 **HIGH PRIORITY**

**Goal**: Improve how producers handle multiple INTEREST messages with different periods.

#### 2.1 Enhanced Period Management

**Files to Modify:**
- `include/api/framework/agent.h`

**New Member Variables:**
```cpp
// For producers (who receive INTEREST and send RESPONSE)
std::vector<std::pair<Address, Microseconds>> _active_periods;  // Track periods per consumer
std::mutex _periods_mutex;                                      // Thread-safe access
Microseconds _computed_gcd_period;                             // Calculated GCD of all periods
std::atomic<bool> _is_producer;                                // Track if this agent is a producer
```

#### 2.2 Implement Enhanced GCD Methods

```cpp
/**
 * @brief Calculate GCD of all active periods from consumers
 * 
 * @return Microseconds The computed GCD period
 */
Microseconds calculate_period_gcd();

/**
 * @brief Add or update a period request from a consumer
 * 
 * @param consumer_address Address of the requesting consumer
 * @param period The requested period
 */
void add_or_update_period_request(const Address& consumer_address, Microseconds period);

/**
 * @brief Remove period requests from a specific consumer
 * 
 * @param consumer_address The consumer address to remove
 */
void remove_period_request(const Address& consumer_address);

/**
 * @brief Get current number of active consumers
 * 
 * @return size_t Number of consumers requesting this data
 */
size_t get_active_consumer_count();
```

#### 2.3 Update handle_interest Method

**Current Location:** Lines 170-184 in `agent.h`

**Replace with enhanced version:**
```cpp
inline void Agent::handle_interest(Unit unit, Microseconds period) {
    db<Agent>(INF) << "[Agent] " << _name << " received INTEREST for unit: " << unit 
                   << " with period: " << period.count() << " microseconds\n";
    
    if (!_is_producer) {
        db<Agent>(WRN) << "[Agent] " << _name << " ignoring INTEREST message (not a producer)\n";
        return;
    }
    
    // Extract consumer address from message context (this needs message modification)
    Address consumer_address = /* extract from current message context */;
    
    add_or_update_period_request(consumer_address, period);
    
    Microseconds new_gcd = calculate_period_gcd();
    
    if (!_periodic_thread) {
        _periodic_thread = new Thread(this, &Agent::reply, unit);
        _periodic_thread->start(new_gcd.count());
    } else {
        _periodic_thread->adjust_period(new_gcd.count());
    }
    
    db<Agent>(INF) << "[Agent] " << _name << " updated response period to GCD: " 
                   << new_gcd.count() << " microseconds for " 
                   << get_active_consumer_count() << " consumers\n";
}
```

#### 2.4 Testing Phase 2

**Test Cases:**
- Multiple consumers requesting same data type with different periods
- GCD calculation correctness
- Consumer addition/removal during runtime
- Producer response rate adaptation

---

### Phase 3: Interest Lifetime Management 🟡 **MEDIUM PRIORITY**

**Goal**: Handle interest message lifecycle and cleanup.

#### 3.1 Interest Registration System

**New Structures:**
```cpp
struct InterestRecord {
    Address consumer_address;
    Microseconds period;
    std::chrono::steady_clock::time_point last_seen;
    std::chrono::steady_clock::time_point expires_at;
    
    InterestRecord(const Address& addr, Microseconds p, std::chrono::seconds ttl = std::chrono::seconds(30))
        : consumer_address(addr), period(p), 
          last_seen(std::chrono::steady_clock::now()),
          expires_at(last_seen + ttl) {}
};

std::vector<InterestRecord> _active_interests;
std::atomic<std::chrono::seconds> _interest_ttl{30}; // 30 second default TTL
Thread* _cleanup_thread;  // Periodic cleanup thread
```

#### 3.2 Cleanup Mechanism

```cpp
/**
 * @brief Periodic cleanup of expired interests
 */
void cleanup_expired_interests();

/**
 * @brief Set the time-to-live for interest messages
 * 
 * @param ttl Time to live in seconds
 */
void set_interest_ttl(std::chrono::seconds ttl);
```

---

### Phase 4: Message Structure Enhancement 🟢 **LOW PRIORITY**

**Goal**: Ensure message structure supports all required fields according to TEDS specification.

#### 4.1 Message Enhancement

**Files to Check/Modify:**
- `include/api/network/message.h`

**Potential Additions:**
- Message sequence numbers
- Interest lifetime/validity information
- Enhanced TEDS code support

---

### Phase 5: Integration and Testing 🔄 **ONGOING**

**Goal**: Comprehensive testing of the periodic interest system.

#### 5.1 Test Strategy

**Unit Tests:**
- `tests/unit_tests/agent_test.cpp` (comprehensive Agent testing including periodic interest)
- `tests/unit_tests/gcd_calculation_test.cpp`
- `tests/unit_tests/interest_lifecycle_test.cpp`

**Integration Tests:**
- `tests/integration_tests/multi_consumer_test.cpp`
- `tests/integration_tests/producer_adaptation_test.cpp`

**Performance Tests:**
- `tests/performance_tests/latency_measurement_test.cpp`
- `tests/performance_tests/throughput_test.cpp`

---

## Implementation Timeline

| Phase | Estimated Time | Dependencies |
|-------|---------------|--------------|
| Phase 1 | 2-3 days | None |
| Phase 2 | 2-3 days | Phase 1 complete |
| Phase 3 | 3-4 days | Phases 1&2 complete |
| Phase 4 | 1-2 days | Analysis of current message format |
| Phase 5 | Ongoing | Each phase as completed |

## Progress Tracking

### Phase 1 Progress
- [x] Add new member variables to Agent class *(Phase 1.1)*
- [x] Update Agent constructor *(Phase 1.2)*
- [x] Implement start_periodic_interest() method *(Phase 1.3)*
- [x] Implement stop_periodic_interest() method *(Phase 1.3)*
- [x] Implement send_interest() method *(Phase 1.3)*
- [x] Implement update_interest_period() method *(Phase 1.3)*
- [x] Update Agent destructor *(Phase 1.4)*
- [x] Update consumer applications *(Phase 1.5)*
- [ ] Create and run unit tests *(Phase 1.6)*

### Phase 2 Progress
- [ ] Add period management member variables
- [ ] Implement enhanced GCD calculation
- [ ] Update handle_interest() method
- [ ] Add consumer tracking functionality
- [ ] Create and run integration tests

### Phase 3 Progress
- [ ] Implement interest record structure
- [ ] Add cleanup mechanism
- [ ] Create TTL management
- [ ] Test lifecycle management

### Phase 4 Progress
- [ ] Analyze current message structure
- [ ] Identify required enhancements
- [ ] Implement message improvements
- [ ] Test TEDS compliance

### Phase 5 Progress
- [ ] Unit test coverage > 90%
- [ ] Integration tests passing
- [ ] Performance benchmarks established
- [ ] Documentation complete

---

## Key Design Decisions

1. **Thread Safety**: All period management uses mutexes for thread-safe operation
2. **Resource Management**: Proper cleanup of periodic threads in destructors  
3. **Backward Compatibility**: Existing Agent API remains functional
4. **Performance**: Minimize overhead in message processing paths
5. **Robustness**: Handle edge cases (zero periods, very short periods, etc.)

---

## Notes and Considerations

- Ensure proper synchronization between interest and response threads
- Consider network bandwidth impact of periodic interest messages
- Monitor system performance under high-frequency interest scenarios
- Plan for graceful degradation when interest periods become too aggressive

---

*This document should be updated as implementation progresses and new requirements are discovered.* 