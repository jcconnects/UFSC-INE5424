# EPOS-Inspired Agent Architecture Migration Plan

## Overview

This document outlines the implementation plan for migrating the current inheritance-based Agent architecture to a composition-based approach inspired by EPOS SmartData principles. This transformation eliminates the "pure virtual method called" race condition and improves system maintainability.

### Current System State
- **Inheritance-based architecture**: All components inherit from Agent base class
- **Virtual method dispatch**: Components override `get()` and `handle_response()` virtual methods
- **Race condition vulnerability**: "pure virtual method called" errors during object destruction
- **vtable dependency**: Runtime polymorphism through virtual function tables

### Target System State
- **Composition-based architecture**: Components become pure data + function pairs
- **Function-based dispatch**: Direct function calls instead of virtual method resolution
- **Thread-safe destruction**: No vtable race conditions during cleanup
- **EPOS SmartData compliance**: Data-centric design following proven embedded patterns

---

## Problem Statement

### The Race Condition
During object destruction, C++ automatically resets the vtable from derived class to base class **before** the base class destructor runs. This creates a critical timing window:

```
Timeline of Crash:
1. ~TestAgent() starts and ends (fast)
2. C++ resets vtable: TestAgent → Agent (automatic)
3. Periodic thread calls reply() → get() → CRASH (pure virtual)
4. ~Agent() starts (too late!)
5. _running = false (too late!)
6. _periodic_thread->join() (too late!)
```

### Impact Scope
This affects **ALL** agent types in the system:
- `CameraComponent`, `LidarComponent`, `INSComponent`
- `ECUComponent`, `BasicProducerA/B`, `BasicConsumerA/B`
- Any future components inheriting from Agent

---

## Solution Architecture

### EPOS SmartData Inspiration
Following EPOS design principles, we transform:
- **Components** → Pure data structures + processing functions
- **Agent** → Execution engine that manages threading, messaging, and data flow
- **Virtual calls** → Direct function pointer invocation
- **Inheritance** → Composition with type-safe interfaces

### Key Design Principles
1. ✅ **Data-centric architecture** (like EPOS SmartData)
2. ✅ **Function-based composition** (no inheritance)
3. ✅ **Type-safe Unit system** (following TSTP patterns)
4. ✅ **Single responsibility** (one Agent per data Unit)
5. ✅ **Resource ownership** (RAII compliance)
6. ✅ **Zero vtable overhead** (pure composition)

---

## Implementation Phases

### Phase 1: Foundation Architecture 🔥 **HIGH PRIORITY** (Week 1)

**Goal**: Create new Agent implementation based on function composition.

#### 1.1 Core Types & Interfaces

**Files to Create:**
- `include/api/framework/agent_v2.h` (new Agent implementation)
- `include/api/framework/component_types.h` (data structures)
- `include/api/framework/component_functions.h` (function signatures)

**New Architecture Components:**
```cpp
// component_types.h
struct ComponentData {
    virtual ~ComponentData() = default;
};

// Function signatures
typedef Agent::Value (*DataProducer)(Agent::Unit unit, ComponentData* data);
typedef void (*ResponseHandler)(Message* msg, ComponentData* data);

// agent_v2.h - Core new Agent class
class Agent {
public:
    Agent(const std::string& name, Unit unit, Type type, Address address,
          DataProducer producer, ResponseHandler handler, 
          std::unique_ptr<ComponentData> data);
    ~Agent();
    
    // Non-virtual interface (eliminates race condition)
    Value get(Unit unit) { return _data_producer(unit, _component_data.get()); }
    void handle_response(Message* msg) { _response_handler(msg, _component_data.get()); }
    
    // Same public interface as current Agent
    int send(Unit unit, Microseconds period);
    int start_periodic_interest(Unit unit, Microseconds period);
    void stop_periodic_interest();
    bool running();
    std::string name() const { return _name; }
    
private:
    std::string _name;
    Unit _unit;
    Type _type;
    std::unique_ptr<ComponentData> _component_data;
    std::function<Value(Unit, ComponentData*)> _data_producer;
    std::function<void(Message*, ComponentData*)> _response_handler;
    
    // Same threading implementation as current Agent
    // But calls function pointers instead of virtual methods
};
```

#### 1.2 Unit Tests for New Architecture

**Files to Create:**
- `tests/unit_tests/agent_v2_test.cpp`

**Critical Test Cases:**
```cpp
class AgentV2Test : public TestCase {
public:
    // Core functionality
    void testAgentV2BasicConstruction();
    void testAgentV2FunctionBasedProducer();
    void testAgentV2FunctionBasedConsumer();
    void testAgentV2ComponentDataOwnership();
    
    // The main problem we're solving
    void testAgentV2ThreadSafety();
    void testAgentV2NoVirtualCallRaceCondition();
    void testAgentV2StressTestDestruction();
};
```

**Success Criteria:**
- ✅ New Agent compiles and passes all basic tests
- ✅ Function-based approach works correctly
- ✅ **Zero "pure virtual method called" errors** in stress tests

---

### Phase 2: Component Data Structures 🔥 **HIGH PRIORITY** (Week 2)

**Goal**: Transform existing components into pure data + function pairs.

#### 2.1 Create Component Data Types

**Files to Create:**
- `include/app/components/basic_producer_data.h`
- `include/app/components/basic_consumer_data.h`
- `include/app/components/camera_data.h`
- `include/app/components/lidar_data.h`
- `include/app/components/ins_data.h`
- `include/app/components/ecu_data.h`

**Example Data Structure:**
```cpp
// camera_data.h
struct CameraData : public ComponentData {
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_real_distribution<> coord_dist;
    std::uniform_real_distribution<> size_dist;
    std::uniform_int_distribution<> label_dist;
    std::vector<std::string> labels;
    
    CameraData() : gen(rd()), 
                   coord_dist(0.0, 1920.0), 
                   size_dist(50.0, 300.0),
                   label_dist(0, 3),
                   labels{"car", "pedestrian", "bicycle", "traffic_light"} {}
};
```

#### 2.2 Create Component Functions

**Files to Create:**
- `src/component/basic_producer_functions.cc`
- `src/component/camera_functions.cc`
- `src/component/lidar_functions.cc`
- `src/component/ins_functions.cc`
- `src/component/ecu_functions.cc`

**Example Function Implementation:**
```cpp
// camera_functions.cc
Agent::Value camera_get_function(Agent::Unit unit, ComponentData* data) {
    CameraData* camera_data = static_cast<CameraData*>(data);
    
    auto now = std::chrono::system_clock::now();
    auto time_us = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();
    
    std::stringstream payload_ss;
    // Same logic as current CameraComponent::get()
    
    std::string msg = "[Camera] " + payload_ss.str() + " at " + std::to_string(time_us);
    return Agent::Value(msg.begin(), msg.end());
}

void camera_handle_response(Message* msg, ComponentData* data) {
    // Default empty implementation for producers
}
```

#### 2.3 Component Function Testing

**Files to Create:**
- `tests/unit_tests/component_functions_test.cpp`

**Test Focus:**
- Function isolation testing
- Data generation correctness
- Memory management validation

---

### Phase 3: Component Migration 🟡 **MEDIUM PRIORITY** (Week 3-4)

**Goal**: Systematically replace inheritance-based components with function-based ones.

#### 3.1 Basic Components Migration (Week 3)

**Migration Order (Low Risk → High Risk):**
1. `BasicProducerA` → function-based approach ✅
2. `BasicProducerB` → function-based approach ✅
3. `BasicConsumerA` → function-based approach ✅
4. `BasicConsumerB` → function-based approach ✅

**Factory Function Pattern:**
```cpp
// basic_producer_a.h
std::unique_ptr<Agent> create_basic_producer_a(
    CAN* can, 
    const Message::Origin& addr, 
    const std::string& name = "BasicProducerA"
) {
    auto data = std::make_unique<BasicProducerData>(0.0f, 100.0f);
    return std::make_unique<Agent>(
        name, 
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Type::INTEREST, 
        addr,
        basic_producer_a_get,
        nullptr, // No response handler for producers
        std::move(data)
    );
}
```

#### 3.2 Complex Components Migration (Week 4)

**Migration Order:**
1. `CameraComponent` → function-based ⚠️
2. `LidarComponent` → function-based ⚠️
3. `INSComponent` → function-based ⚠️
4. `ECUComponent` → function-based 🔴 **MOST COMPLEX**

**Special Considerations:**
- **ECUComponent**: Multi-unit consumer requires multiple response handlers
- **Complex data generation**: Ensure mathematical correctness preserved
- **Performance**: Function calls should match or exceed virtual call performance

---

### Phase 4: Test Migration & API Replacement 🟢 **LOW PRIORITY** (Week 5)

**Goal**: Complete API replacement and comprehensive testing.

#### 4.1 Replace agent.h with New Implementation

**Files to Update:**
- `include/api/framework/agent.h` (replace with agent_v2.h content)
- Remove `agent_v2.h` (merge complete)

#### 4.2 Comprehensive Test Migration

**Files to Update:**
- `tests/unit_tests/agent_test.cpp` - **Complete rewrite**

**Critical Test: Race Condition Verification**
```cpp
void AgentTest::testAgentThreadSafetyWithFunctions() {
    auto producer = create_basic_producer_a(_test_can.get(), {}, "TestProducer");
    auto consumer = create_basic_consumer_a(_test_can.get(), {}, "TestConsumer");
    
    consumer->start_periodic_interest(
        static_cast<std::uint32_t>(DataTypes::UNIT_A), 
        Agent::Microseconds(100000)
    );
    
    // Stress test: rapid creation/destruction (this used to crash!)
    for (int i = 0; i < 100; ++i) {
        auto temp_producer = create_basic_producer_a(_test_can.get(), {}, "TempProducer");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        // temp_producer destructor called here - should NOT crash anymore
    }
    
    consumer->stop_periodic_interest();
    // SUCCESS: No "pure virtual method called" errors!
}
```

#### 4.3 Vehicle Class Updates

**Files to Update:**
- Update `Vehicle` class to use factory functions
- Replace inheritance storage with composition

```cpp
// New Vehicle implementation
class Vehicle {
    std::vector<std::unique_ptr<Agent>> _components; // Same container type!
    
public:
    void add_camera() {
        _components.push_back(create_camera_component(_can, _origin, "Camera"));
    }
    
    void add_lidar() {
        _components.push_back(create_lidar_component(_can, _origin, "Lidar"));
    }
    
    // Interface unchanged - composition under the hood!
};
```

---

### Phase 5: Final Validation & Cleanup 🎯 **CRITICAL** (Week 6)

**Goal**: Verify complete problem resolution and system stability.

#### 5.1 Original Problem Verification

**Critical Validation Tests:**
```bash
# Run the original failing test 100 times
for i in {1..100}; do
    ./agent_test testPeriodicInterestWithMessageFlow
    if [ $? -ne 0 ]; then
        echo "FAILED on iteration $i"
        exit 1
    fi
done
echo "SUCCESS: All 100 iterations passed!"
```

#### 5.2 Performance Validation

**Benchmarks to Run:**
- **Message latency**: Function calls vs virtual calls
- **Memory usage**: Reduced vtable overhead
- **Throughput**: Messages per second processing rate
- **Creation overhead**: Agent instantiation performance

#### 5.3 Documentation & Migration Guide

**Files to Create:**
- `docs/architecture_migration_guide.md`
- `docs/component_development_guide.md`
- Update main README with new patterns

#### 5.4 Code Cleanup

**Cleanup Tasks:**
- Remove old inheritance-based component classes
- Remove unused virtual method declarations
- Clean up test files and remove legacy patterns
- Update all example code and documentation

---

## Progress Tracking

### Phase 1 Progress (Week 1)
- [x] Create core types and interfaces *(1.1)*
- [x] Implement new Agent class *(1.1)*
- [x] Create comprehensive unit tests *(1.2)*
- [x] Verify basic functionality *(1.2)*

### Phase 2 Progress (Week 2)
- [ ] Create all component data structures *(2.1)*
- [ ] Implement all component functions *(2.2)*
- [ ] Component function unit tests *(2.3)*
- [ ] Validate data processing correctness *(2.3)*

### Phase 3 Progress (Week 3-4)
- [ ] Migrate BasicProducer/Consumer components *(3.1)*
- [ ] Create factory functions *(3.1)*
- [ ] Migrate complex components *(3.2)*
- [ ] Integration testing after each migration *(3.2)*

### Phase 4 Progress (Week 5)
- [ ] Replace agent.h with new implementation *(4.1)*
- [ ] Complete test migration *(4.2)*
- [ ] Update Vehicle class *(4.3)*
- [ ] Verify API compatibility *(4.3)*

### Phase 5 Progress (Week 6)
- [ ] Run original problem verification *(5.1)*
- [ ] Performance benchmarking *(5.2)*
- [ ] Documentation updates *(5.3)*
- [ ] Final code cleanup *(5.4)*

---

## Success Criteria

### Primary Objectives
1. ✅ **Zero "pure virtual method called" errors** in any scenario
2. ✅ **Complete API compatibility** - existing applications work unchanged
3. ✅ **Same or better performance** than inheritance-based architecture
4. ✅ **All existing tests pass** with new implementation

### Secondary Objectives
1. ✅ **Reduced memory footprint** (no vtables)
2. ✅ **Improved maintainability** (clear separation of data and behavior)
3. ✅ **Better testability** (functions easier to unit test)
4. ✅ **EPOS compliance** (follows proven embedded design patterns)

### Verification Methods
- **Stress testing**: 1000+ rapid agent creation/destruction cycles
- **Memory profiling**: Valgrind leak detection and usage analysis
- **Performance benchmarking**: Latency and throughput measurements
- **Integration testing**: Full system tests with Vehicle class

---

## Risk Mitigation

### Identified Risks
1. **Performance regression**: Function calls might be slower than expected
2. **Memory management**: Component data lifecycle complexity
3. **Migration complexity**: Large codebase changes introduce bugs
4. **API compatibility**: Subtle interface changes break applications

### Mitigation Strategies
1. **Gradual migration**: Phase-by-phase approach reduces risk
2. **Comprehensive testing**: Each phase has dedicated test coverage
3. **Performance monitoring**: Benchmarks at each phase
4. **Rollback capability**: Not production system - full revert possible

---

## Architecture Comparison

### Before (Inheritance-Based)
```cpp
class CameraComponent : public Agent {
public:
    Agent::Value get(Agent::Unit unit) override; // VIRTUAL CALL
};

// Problem: vtable reset during destruction creates race condition
```

### After (Composition-Based)
```cpp
struct CameraData : ComponentData { /* pure data */ };

Agent::Value camera_get_function(Agent::Unit unit, ComponentData* data) {
    // DIRECT FUNCTION CALL - no vtable involved
}

std::unique_ptr<Agent> create_camera_component(...) {
    return std::make_unique<Agent>(..., camera_get_function, ...);
}

// Solution: no inheritance, no vtables, no race conditions
```

---

## Timeline Overview

| Phase | Duration | Key Deliverables | Risk Level |
|-------|----------|------------------|------------|
| Phase 1 | Week 1 | New Agent architecture + tests | 🔴 High |
| Phase 2 | Week 2 | Component data structures + functions | 🟡 Medium |
| Phase 3 | Week 3-4 | Component migration (basic→complex) | 🟡 Medium |
| Phase 4 | Week 5 | API replacement + test migration | 🟢 Low |
| Phase 5 | Week 6 | Validation + cleanup + documentation | 🟢 Low |

**Total Estimated Duration**: 6 weeks
**Confidence Level**: High (based on EPOS proven patterns)

---

## References

- **EPOS SmartData Documentation**: Embedded Parallel Operating System patterns
- **IEEE 1451 TEDS**: Transducer Electronic Data Sheets specification
- **C++ vtable mechanics**: Understanding virtual function dispatch during destruction
- **TSTP Protocol**: Trustful Space-Time Protocol for data-centric communication

---

*This document should be updated as implementation progresses and new requirements are discovered. The migration follows proven EPOS design patterns to ensure reliability and maintainability.*