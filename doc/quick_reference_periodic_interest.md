# Quick Reference: Periodic Interest Implementation

## 🔥 Phase 1: Consumer Periodic Interest (HIGH PRIORITY)

### Files to Modify
- `include/api/framework/agent.h` (lines 67-84, after line 263)

### Key Changes
1. **Add new members:**
   ```cpp
   Thread* _interest_thread;
   Microseconds _requested_period;
   std::atomic<bool> _interest_active;
   std::atomic<bool> _is_consumer;
   ```

2. **Replace constructor code:**
   ```cpp
   // OLD: send(unit, period); 
   // NEW: _is_consumer = (type == Type::RESPONSE);
   ```

3. **Add new methods:**
   - `start_periodic_interest(Unit unit, Microseconds period)`
   - `stop_periodic_interest()`
   - `send_interest(Unit unit)`

### Test Command
```bash
make bin/unit_tests/periodic_interest_test && ./bin/unit_tests/periodic_interest_test
```

---

## 🔥 Phase 2: Producer GCD Enhancement (HIGH PRIORITY)

### Key Data Structures
```cpp
std::vector<std::pair<Address, Microseconds>> _active_periods;
std::mutex _periods_mutex;
Microseconds _computed_gcd_period;
```

### Critical Method
- Update `handle_interest()` in lines 170-184 of `agent.h`

---

## Current vs Target System

| Aspect | Current | Target |
|--------|---------|--------|
| Interest Frequency | Once at startup | Periodic (configurable) |
| Period Management | Basic GCD | Enhanced multi-consumer GCD |
| Lifetime | Infinite | TTL-based expiration |
| Consumer Control | Automatic | Explicit start/stop |

---

## Progress Checklist (Phase 1)
- [ ] ✏️ Add new Agent member variables
- [ ] 🔧 Implement `start_periodic_interest()`
- [ ] 🔧 Implement `stop_periodic_interest()`  
- [ ] 🔧 Implement `send_interest()`
- [ ] ⚙️ Update Agent constructor
- [ ] ⚙️ Update Agent destructor
- [ ] 📝 Update consumer applications
- [ ] 🧪 Create and run unit tests

## Next Steps
1. Start with Phase 1.1 (extend Agent class)
2. Test each method individually
3. Move to Phase 2 only after Phase 1 is complete
4. Use progress checkboxes in main document

---

*Last Updated: [Date] | See full plan: doc/implementation_plan_periodic_interest.md* 