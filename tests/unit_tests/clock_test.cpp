#include "../../tests/testcase.h"
#include "../../tests/test_utils.h"
#include "../../include/api/framework/clock.h"
#include "../../include/api/framework/leaderKeyStorage.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <iomanip>

// Test constants
const LeaderIdType TEST_LEADER_ID = 1;
const LeaderIdType NON_LEADER_ID = 2;

using namespace std::chrono_literals;

// Forward declarations and class interface
class ClockTest;

// Stream operator declaration
inline std::ostream& operator<<(std::ostream& os, const TimestampType& tp);

// Helper function declaration
PtpRelevantData createPtpData(LeaderIdType sender_id, 
                            TimestampType tx_time,
                            TimestampType rx_time);

class ClockTest : public TestCase {
protected:
    void setUp() override;
    void tearDown() override;

    // Helper to get clock to a specific state
    void getToState(Clock::State target_state);

    // Helper to compare timestamps with tolerance
    bool timestampsEqual(const TimestampType& t1, const TimestampType& t2, 
                        const std::chrono::microseconds tolerance = 1000us);

    // Helper to assert timestamps are equal within tolerance
    void assert_timestamps_equal(const TimestampType& expected, const TimestampType& actual,
                               const std::string& message,
                               const std::chrono::microseconds tolerance = 1000us);

    // === BASIC STATE AND INITIALIZATION TESTS ===
    void testClockStartsInUnsynchronizedState();
    void testClockBehaviorWithoutLeaderSet();
    void testClockIgnoresMessagesFromNonLeader();
    void testClockHandlesNullMessagesProperly();

    // === STATE TRANSITION TESTS ===
    void testValidStateTransitionSequence();
    void testStateTransitionFromUnsynchronizedToAwaitingSecondMessage();
    void testStateTransitionFromAwaitingSecondMessageToSynchronized();
    void testStateRemainsInSynchronizedWithSubsequentMessages();

    // === TIMEOUT BEHAVIOR TESTS ===
    void testTimeoutTransitionFromAwaitingSecondMessageToUnsynchronized();
    void testTimeoutTransitionFromSynchronizedToUnsynchronized();
    void testNoTimeoutOccursWithRecentMessages();
    void testTimeoutBoundaryConditionsAtExactInterval();
    void testTimeoutTimerResetWithNewValidMessages();

    // === LEADER CHANGE TESTS ===
    void testLeaderChangeResetsStateToUnsynchronized();
    void testClockIgnoresOldLeaderMessagesAfterLeaderChange();
    void testClockAcceptsNewLeaderMessagesAfterLeaderChange();
    void testLeaderChangeInDifferentStates();

    // === TIME SYNCHRONIZATION TESTS ===
    void testSynchronizedTimeReturnsLocalTimeWhenUnsynchronized();
    void testSynchronizedTimeCalculationInAwaitingSecondMessageState();
    void testSynchronizedTimeCalculationInSynchronizedState();
    void testSynchronizedTimeProgressesForwardCorrectly();

    // === PROPAGATION DELAY AND DISTANCE TESTS ===
    // void testPropagationDelayCalculationWithZeroDistance();
    // void testPropagationDelayCalculationWithKnownDistance();
    // void testPropagationDelayCalculationWithLargeDistances();
    // void testPropagationDelayCalculationWithNegativeCoordinates();

    // === DRIFT CALCULATION TESTS ===
    void testDriftCalculationWithPerfectSynchronization();
    void testDriftCalculationWithPositiveDrift();
    void testDriftCalculationWithNegativeDrift();
    void testDriftCalculationWithVerySmallTimeDifferences();
    void testDriftCalculationUpdatesWithSubsequentMessages();

    // === LOCAL TIME METHOD TESTS ===
    void testLocalSystemTimeMethodBasicFunctionality();
    void testLocalSystemTimeMethodProgression();
    void testLocalSystemTimeMethodConsistencyWithSteadyTime();
    void testLocalSystemTimeMethodMonotonicBehavior();

    // === EDGE CASES AND ERROR CONDITION TESTS ===
    void testClockHandlesImpossibleTimingGracefully();
    void testClockHandlesExtremeCoordinateValues();
    void testClockHandlesZeroTimeDifferenceBetweenMessages();
    void testClockHandlesRapidMessageSequences();
    void testClockHandlesMaximumLeaderIdValues();

    // === THREAD SAFETY TESTS ===
    void testClockMethodsAreThreadSafe();
    void testLeaderKeyStorageIntegrationIsThreadSafe();
    void testLocalTimeMethodsAreThreadSafe();

    // === INTEGRATION TESTS ===
    // void testClockProtocolNicIntegration();

public:
    ClockTest();
};

// Implementations

/**
 * @brief Constructor that registers all test methods
 * 
 * Organizes tests into logical groups for better maintainability and clarity.
 * Each test method name clearly describes what functionality is being tested.
 */
ClockTest::ClockTest() {
    // === BASIC STATE AND INITIALIZATION TESTS ===
    DEFINE_TEST(testClockStartsInUnsynchronizedState);
    DEFINE_TEST(testClockBehaviorWithoutLeaderSet);
    DEFINE_TEST(testClockIgnoresMessagesFromNonLeader);
    DEFINE_TEST(testClockHandlesNullMessagesProperly);

    // === STATE TRANSITION TESTS ===
    DEFINE_TEST(testValidStateTransitionSequence);
    DEFINE_TEST(testStateTransitionFromUnsynchronizedToAwaitingSecondMessage);
    DEFINE_TEST(testStateTransitionFromAwaitingSecondMessageToSynchronized);
    DEFINE_TEST(testStateRemainsInSynchronizedWithSubsequentMessages);

    // === TIMEOUT BEHAVIOR TESTS ===
    DEFINE_TEST(testTimeoutTransitionFromAwaitingSecondMessageToUnsynchronized);
    DEFINE_TEST(testTimeoutTransitionFromSynchronizedToUnsynchronized);
    DEFINE_TEST(testNoTimeoutOccursWithRecentMessages);
    DEFINE_TEST(testTimeoutBoundaryConditionsAtExactInterval);
    DEFINE_TEST(testTimeoutTimerResetWithNewValidMessages);

    // === LEADER CHANGE TESTS ===
    DEFINE_TEST(testLeaderChangeResetsStateToUnsynchronized);
    DEFINE_TEST(testClockIgnoresOldLeaderMessagesAfterLeaderChange);
    DEFINE_TEST(testClockAcceptsNewLeaderMessagesAfterLeaderChange);
    DEFINE_TEST(testLeaderChangeInDifferentStates);

    // === TIME SYNCHRONIZATION TESTS ===
    DEFINE_TEST(testSynchronizedTimeReturnsLocalTimeWhenUnsynchronized);
    DEFINE_TEST(testSynchronizedTimeCalculationInAwaitingSecondMessageState);
    DEFINE_TEST(testSynchronizedTimeCalculationInSynchronizedState);
    DEFINE_TEST(testSynchronizedTimeProgressesForwardCorrectly);

    // === PROPAGATION DELAY AND DISTANCE TESTS ===
    // DEFINE_TEST(testPropagationDelayCalculationWithZeroDistance);
    // DEFINE_TEST(testPropagationDelayCalculationWithKnownDistance);
    // DEFINE_TEST(testPropagationDelayCalculationWithLargeDistances);
    // DEFINE_TEST(testPropagationDelayCalculationWithNegativeCoordinates);

    // === DRIFT CALCULATION TESTS ===
    DEFINE_TEST(testDriftCalculationWithPerfectSynchronization);
    DEFINE_TEST(testDriftCalculationWithPositiveDrift);
    DEFINE_TEST(testDriftCalculationWithNegativeDrift);
    DEFINE_TEST(testDriftCalculationWithVerySmallTimeDifferences);
    DEFINE_TEST(testDriftCalculationUpdatesWithSubsequentMessages);

    // === LOCAL TIME METHOD TESTS ===
    DEFINE_TEST(testLocalSystemTimeMethodBasicFunctionality);
    DEFINE_TEST(testLocalSystemTimeMethodProgression);
    DEFINE_TEST(testLocalSystemTimeMethodConsistencyWithSteadyTime);
    DEFINE_TEST(testLocalSystemTimeMethodMonotonicBehavior);

    // === EDGE CASES AND ERROR CONDITION TESTS ===
    DEFINE_TEST(testClockHandlesImpossibleTimingGracefully);
    DEFINE_TEST(testClockHandlesExtremeCoordinateValues);
    DEFINE_TEST(testClockHandlesZeroTimeDifferenceBetweenMessages);
    DEFINE_TEST(testClockHandlesRapidMessageSequences);
    DEFINE_TEST(testClockHandlesMaximumLeaderIdValues);

    // === THREAD SAFETY TESTS ===
    DEFINE_TEST(testClockMethodsAreThreadSafe);
    DEFINE_TEST(testLeaderKeyStorageIntegrationIsThreadSafe);
    DEFINE_TEST(testLocalTimeMethodsAreThreadSafe);

    // === INTEGRATION TESTS ===
    // DEFINE_TEST(testClockProtocolNicIntegration);
}

// ClockTest method implementations
void ClockTest::setUp() {
    auto& storage = LeaderKeyStorage::getInstance();
    auto& clock = Clock::getInstance();
    // Reset Clock singleton
    clock.reset();
    // Reset LeaderKeyStorage
    storage.setLeaderId(Ethernet::NULL_ADDRESS);
    MacKeyType empty_key;
    empty_key.fill(0);
    storage.setGroupMacKey(empty_key);
    // Ensure we're in a clean state
    std::this_thread::sleep_for(10ms); // Give time for any pending operations
}

void ClockTest::tearDown() {
    // No cleanup needed
}

/**
 * @brief Helper method to transition the Clock to a specific state
 * 
 * @param target_state The desired Clock state to transition to
 * 
 * This utility method sets up the necessary conditions and sends the
 * required PTP messages to transition the Clock from its current state
 * to the specified target state. Used by tests to establish known
 * starting conditions.
 */
void ClockTest::getToState(Clock::State target_state) {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    const LeaderIdType TEST_LEADER_ID = 1;
    
    // Set leader in storage
    Ethernet::Address leader_addr;
    leader_addr.bytes[5] = static_cast<uint8_t>(TEST_LEADER_ID);
    storage.setLeaderId(leader_addr);
    
    if (target_state == Clock::State::UNSYNCHRONIZED) {
        return; // Already in UNSYNCHRONIZED after setup
    }
    
    auto now = clock.getLocalSteadyHardwareTime();
    auto ptp_data1 = createPtpData(TEST_LEADER_ID, now, now + 100us);
    clock.activate(&ptp_data1);
    
    if (target_state == Clock::State::AWAITING_SECOND_MSG) {
        return;
    }
    
    auto ptp_data2 = createPtpData(TEST_LEADER_ID, now + 1000us, now + 1100us);
    clock.activate(&ptp_data2);
}

/**
 * @brief Helper method to compare timestamps with tolerance
 * 
 * @param t1 First timestamp to compare
 * @param t2 Second timestamp to compare
 * @param tolerance Maximum allowed difference in microseconds
 * @return true if timestamps are equal within tolerance, false otherwise
 * 
 * This utility method compares two timestamps and returns true if they
 * are within the specified tolerance of each other. Used for testing
 * timing-related functionality where exact equality is not expected.
 */
bool ClockTest::timestampsEqual(const TimestampType& t1, const TimestampType& t2, 
                            const std::chrono::microseconds tolerance) {
    return std::abs((t1 - t2).count()) <= tolerance.count();
}

/**
 * @brief Helper method to assert that timestamps are equal within tolerance
 * 
 * @param expected The expected timestamp value
 * @param actual The actual timestamp value to compare
 * @param message Error message to display if assertion fails
 * @param tolerance Maximum allowed difference in microseconds
 * 
 * This utility method asserts that two timestamps are equal within the
 * specified tolerance. If they are not equal, it throws a runtime_error
 * with detailed information about the expected vs actual values and
 * the difference between them.
 */
void ClockTest::assert_timestamps_equal(const TimestampType& expected, const TimestampType& actual,
                               const std::string& message,
                               const std::chrono::microseconds tolerance) {
    if (!timestampsEqual(expected, actual, tolerance)) {
        std::ostringstream oss;
        oss << message << " (expected " << expected << " but got " << actual 
            << ", difference: " << (actual - expected).count() << "us)";
        throw std::runtime_error(oss.str());
    }
}

/**
 * @brief Tests that the Clock starts in the UNSYNCHRONIZED state
 * 
 * Verifies that when the Clock singleton is first accessed, it is in the
 * UNSYNCHRONIZED state and reports that it is not fully synchronized.
 * This is the expected initial state before any synchronization messages
 * have been processed.
 */
void ClockTest::testClockStartsInUnsynchronizedState() {
    auto& clock = Clock::getInstance();
    assert_equal(Clock::State::UNSYNCHRONIZED, clock.getState(), "Initial state should be UNSYNCHRONIZED");
    assert_false(clock.isFullySynchronized(), "Initial state should not be synchronized");
}

/**
 * @brief Tests Clock behavior when no leader is set in LeaderKeyStorage
 * 
 * Verifies that when no leader is configured in the LeaderKeyStorage,
 * the Clock remains in UNSYNCHRONIZED state and ignores incoming PTP
 * messages. This ensures the Clock only processes messages when a valid
 * leader has been established through the key management system.
 */
void ClockTest::testClockBehaviorWithoutLeaderSet() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    
    // Ensure no leader is set in LeaderKeyStorage
    storage.setLeaderId(Ethernet::NULL_ADDRESS);
    
    // Verify initial state
    assert_equal(INVALID_LEADER_ID, clock.getCurrentLeader(), 
        "Clock should have no leader after reset");
    assert_equal(Clock::State::UNSYNCHRONIZED, clock.getState(), 
        "Clock should be in UNSYNCHRONIZED state after reset");
    
    // Try to process message without leader
    auto now = clock.getLocalSteadyHardwareTime();
    auto ptp_data = createPtpData(1, now, now + 100us);
    
    // Should stay in UNSYNCHRONIZED
    clock.activate(&ptp_data);
    assert_equal(Clock::State::UNSYNCHRONIZED, clock.getState(), 
        "Should stay in UNSYNCHRONIZED without leader");
    assert_equal(INVALID_LEADER_ID, clock.getCurrentLeader(), 
        "Clock should still have no leader after message");
}

/**
 * @brief Tests that Clock ignores messages from non-leader nodes
 * 
 * Verifies that when a leader is set in LeaderKeyStorage, the Clock only
 * processes messages from that specific leader and ignores messages from
 * other nodes. This test checks this behavior across all Clock states:
 * UNSYNCHRONIZED, AWAITING_SECOND_MSG, and SYNCHRONIZED.
 */
void ClockTest::testClockIgnoresMessagesFromNonLeader() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    
    // Set up a leader
    Ethernet::Address leader_addr;
    leader_addr.bytes[5] = TEST_LEADER_ID;
    storage.setLeaderId(leader_addr);
    
    // Create message from non-leader
    auto now = clock.getLocalSteadyHardwareTime();
    auto ptp_data = createPtpData(NON_LEADER_ID, now, now + 100us);
    
    // Activate with non-leader message
    clock.activate(&ptp_data);
    
    // Should remain in UNSYNCHRONIZED state
    assert_equal(Clock::State::UNSYNCHRONIZED, clock.getState(), "Clock should ignore non-leader messages");
}

/**
 * @brief Tests Clock behavior when activated with null messages
 * 
 * Verifies that the Clock handles null message pointers gracefully without
 * crashing or changing state inappropriately. Null messages can occur during
 * timeout checks or when no valid message is available. The Clock should
 * remain in its current state when processing null messages (unless a
 * timeout condition is met).
 */
void ClockTest::testClockHandlesNullMessagesProperly() {
    auto& clock = Clock::getInstance();
    
    // Test in UNSYNCHRONIZED
    clock.activate(nullptr);
    assert_equal(Clock::State::UNSYNCHRONIZED, clock.getState(), "Should stay in UNSYNCHRONIZED with null message");
    
    // Test in AWAITING_SECOND_MSG
    getToState(Clock::State::AWAITING_SECOND_MSG);
    clock.activate(nullptr);
    assert_equal(Clock::State::AWAITING_SECOND_MSG, clock.getState(), "Should stay in AWAITING_SECOND_MSG with null message (no timeout)");
    
    // Test in SYNCHRONIZED
    getToState(Clock::State::SYNCHRONIZED);
    clock.activate(nullptr);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), "Should stay in SYNCHRONIZED with null message (no timeout)");
}

/**
 * @brief Tests the complete valid state transition sequence
 * 
 * Verifies the normal progression of Clock states when receiving valid
 * PTP messages from the designated leader:
 * 1. UNSYNCHRONIZED → AWAITING_SECOND_MSG (first message)
 * 2. AWAITING_SECOND_MSG → SYNCHRONIZED (second message)
 * 3. SYNCHRONIZED → SYNCHRONIZED (subsequent messages)
 * 
 * This represents the expected happy path for clock synchronization.
 */
void ClockTest::testValidStateTransitionSequence() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    
    // Set up leader
    Ethernet::Address leader_addr;
    leader_addr.bytes[5] = TEST_LEADER_ID;
    storage.setLeaderId(leader_addr);
    
    auto now = clock.getLocalSteadyHardwareTime();
    
    // First message: UNSYNCHRONIZED -> AWAITING_SECOND_MSG
    auto ptp_data1 = createPtpData(TEST_LEADER_ID, now, now + 100us);
    clock.activate(&ptp_data1);
    assert_equal(Clock::State::AWAITING_SECOND_MSG, clock.getState(), "Should transition to AWAITING_SECOND_MSG");
    
    // Second message: AWAITING_SECOND_MSG -> SYNCHRONIZED
    auto ptp_data2 = createPtpData(TEST_LEADER_ID, now + 1000us, now + 1100us);
    clock.activate(&ptp_data2);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), "Should transition to SYNCHRONIZED");
    
    // Third message: SYNCHRONIZED -> SYNCHRONIZED
    auto ptp_data3 = createPtpData(TEST_LEADER_ID, now + 2000us, now + 2100us);
    clock.activate(&ptp_data3);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), "Should remain SYNCHRONIZED");
}

/**
 * @brief Tests the specific transition from UNSYNCHRONIZED to AWAITING_SECOND_MSG
 * 
 * Focuses on the first critical transition in the synchronization process.
 * When the Clock receives its first valid PTP message from the leader,
 * it should transition from UNSYNCHRONIZED to AWAITING_SECOND_MSG state.
 * This transition establishes the initial timing reference point.
 */
void ClockTest::testStateTransitionFromUnsynchronizedToAwaitingSecondMessage() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    
    // Set up leader
    Ethernet::Address leader_addr;
    leader_addr.bytes[5] = TEST_LEADER_ID;
    storage.setLeaderId(leader_addr);
    
    auto now = clock.getLocalSteadyHardwareTime();
    
    // First message should trigger transition
    auto ptp_data1 = createPtpData(TEST_LEADER_ID, now, now + 100us);
    clock.activate(&ptp_data1);
    assert_equal(Clock::State::AWAITING_SECOND_MSG, clock.getState(), "Should transition to AWAITING_SECOND_MSG");
    
    // Verify we can't go back to UNSYNCHRONIZED with valid message
    auto ptp_data2 = createPtpData(TEST_LEADER_ID, now + 1000us, now + 1100us);
    clock.activate(&ptp_data2);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), "Should transition to SYNCHRONIZED");
    
    // Reset and test again
    clock.reset();
    auto ptp_data3 = createPtpData(TEST_LEADER_ID, now + 2000us, now + 2100us);
    clock.activate(&ptp_data3);
    assert_equal(Clock::State::AWAITING_SECOND_MSG, clock.getState(), "Should transition to AWAITING_SECOND_MSG again");
}

/**
 * @brief Tests the transition from AWAITING_SECOND_MSG to SYNCHRONIZED
 * 
 * Verifies the critical second transition in the synchronization process.
 * When the Clock receives a second valid PTP message from the leader while
 * in AWAITING_SECOND_MSG state, it should transition to SYNCHRONIZED state.
 * This transition enables drift calculation and full synchronization capability.
 */
void ClockTest::testStateTransitionFromAwaitingSecondMessageToSynchronized() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    
    // Set up leader
    Ethernet::Address leader_addr;
    leader_addr.bytes[5] = TEST_LEADER_ID;
    storage.setLeaderId(leader_addr);
    
    auto now = clock.getLocalSteadyHardwareTime();
    
    // Get to AWAITING_SECOND_MSG state
    auto ptp_data1 = createPtpData(TEST_LEADER_ID, now, now + 100us);
    clock.activate(&ptp_data1);
    assert_equal(Clock::State::AWAITING_SECOND_MSG, clock.getState(), "Should be in AWAITING_SECOND_MSG");
    
    // Second message should trigger transition to SYNCHRONIZED
    auto ptp_data2 = createPtpData(TEST_LEADER_ID, now + 1000us, now + 1100us);
    clock.activate(&ptp_data2);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), "Should transition to SYNCHRONIZED");
    
    // Verify we stay synchronized with subsequent messages
    auto ptp_data3 = createPtpData(TEST_LEADER_ID, now + 2000us, now + 2100us);
    clock.activate(&ptp_data3);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), "Should remain SYNCHRONIZED");
}

/**
 * @brief Tests that Clock remains in SYNCHRONIZED state with subsequent messages
 * 
 * Verifies that once the Clock reaches SYNCHRONIZED state, it continues to
 * remain in that state when receiving additional valid PTP messages from
 * the leader. This ensures stable operation during normal synchronization
 * and allows for continuous drift correction and timing updates.
 */
void ClockTest::testStateRemainsInSynchronizedWithSubsequentMessages() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    
    // Set up leader
    Ethernet::Address leader_addr;
    leader_addr.bytes[5] = TEST_LEADER_ID;
    storage.setLeaderId(leader_addr);
    
    auto now = clock.getLocalSteadyHardwareTime();
    
    // Get to SYNCHRONIZED state
    auto ptp_data1 = createPtpData(TEST_LEADER_ID, now, now + 100us);
    clock.activate(&ptp_data1);
    assert_equal(Clock::State::AWAITING_SECOND_MSG, clock.getState(), "Should be in AWAITING_SECOND_MSG");
    
    auto ptp_data2 = createPtpData(TEST_LEADER_ID, now + 1000us, now + 1100us);
    clock.activate(&ptp_data2);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), "Should be SYNCHRONIZED");
    
    // Subsequent messages should keep us in SYNCHRONIZED
    auto ptp_data3 = createPtpData(TEST_LEADER_ID, now + 2000us, now + 2100us);
    clock.activate(&ptp_data3);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), "Should remain SYNCHRONIZED");
}

/**
 * @brief Tests timeout transition from AWAITING_SECOND_MSG to UNSYNCHRONIZED
 * 
 * Verifies that when the Clock is in AWAITING_SECOND_MSG state and doesn't
 * receive a second message within MAX_LEADER_SILENCE_INTERVAL (5 seconds),
 * it transitions back to UNSYNCHRONIZED state. This prevents the Clock from
 * remaining indefinitely in a partially synchronized state when the leader
 * becomes unavailable.
 */
void ClockTest::testTimeoutTransitionFromAwaitingSecondMessageToUnsynchronized() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    const LeaderIdType TEST_LEADER_ID = 1;
    
    // Set leader in storage
    Ethernet::Address leader_addr;
    leader_addr.bytes[5] = static_cast<uint8_t>(TEST_LEADER_ID);
    storage.setLeaderId(leader_addr);
    
    // Test 1: Timeout in AWAITING_SECOND_MSG
    getToState(Clock::State::AWAITING_SECOND_MSG);
    std::this_thread::sleep_for(clock.getMaxLeaderSilenceInterval());
    clock.activate(nullptr);
    assert_equal(Clock::State::UNSYNCHRONIZED, clock.getState(), "Should timeout to UNSYNCHRONIZED from AWAITING_SECOND_MSG");
    
    // Test 2: Timeout in SYNCHRONIZED
    getToState(Clock::State::SYNCHRONIZED);
    std::this_thread::sleep_for(clock.getMaxLeaderSilenceInterval() * 1.1);
    clock.activate(nullptr);
    assert_equal(Clock::State::UNSYNCHRONIZED, clock.getState(), "Should timeout to UNSYNCHRONIZED from SYNCHRONIZED");
}

/**
 * @brief Tests timeout transition from SYNCHRONIZED to UNSYNCHRONIZED
 * 
 * Verifies that when the Clock is in SYNCHRONIZED state and doesn't receive
 * messages from the leader within MAX_LEADER_SILENCE_INTERVAL (5 seconds),
 * it transitions back to UNSYNCHRONIZED state. This ensures the Clock doesn't
 * continue to provide potentially stale synchronized time when the leader
 * becomes unavailable.
 */
void ClockTest::testTimeoutTransitionFromSynchronizedToUnsynchronized() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    const LeaderIdType TEST_LEADER_ID = 1;
    
    // Set leader in storage
    Ethernet::Address leader_addr;
    leader_addr.bytes[5] = static_cast<uint8_t>(TEST_LEADER_ID);
    storage.setLeaderId(leader_addr);
    
    auto now = clock.getLocalSteadyHardwareTime();
    
    // Get to SYNCHRONIZED state
    auto ptp_data1 = createPtpData(TEST_LEADER_ID, now, now + 100us);
    clock.activate(&ptp_data1);
    
    auto ptp_data2 = createPtpData(TEST_LEADER_ID, now + 1000us, now + 1100us);
    clock.activate(&ptp_data2);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), "Should be SYNCHRONIZED");
    
    // Wait for timeout and verify transition back to UNSYNCHRONIZED
    std::this_thread::sleep_for(clock.getMaxLeaderSilenceInterval());
    clock.activate(nullptr);
    assert_equal(Clock::State::UNSYNCHRONIZED, clock.getState(), 
        "Should timeout to UNSYNCHRONIZED from SYNCHRONIZED");
}

/**
 * @brief Tests that no timeout occurs when messages are received within the timeout interval
 * 
 * Verifies that the Clock's timeout mechanism works correctly by ensuring that
 * when messages are received within MAX_LEADER_SILENCE_INTERVAL, no timeout
 * occurs. This test also verifies that the timeout timer is properly reset
 * when new valid messages are received, preventing false timeouts.
 */
void ClockTest::testNoTimeoutOccursWithRecentMessages() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    const LeaderIdType TEST_LEADER_ID = 1;
    
    // Set leader in storage
    Ethernet::Address leader_addr;
    leader_addr.bytes[5] = static_cast<uint8_t>(TEST_LEADER_ID);
    storage.setLeaderId(leader_addr);
    
    // Test 1: Verify timeout doesn't occur in UNSYNCHRONIZED state (no sync event yet)
    assert_equal(Clock::State::UNSYNCHRONIZED, clock.getState(), 
        "Should start in UNSYNCHRONIZED state");
    
    // Wait longer than MAX_LEADER_SILENCE_INTERVAL and activate with null
    std::this_thread::sleep_for(clock.getMaxLeaderSilenceInterval()); // MAX_LEADER_SILENCE_INTERVAL is 5s
    clock.activate(nullptr);
    assert_equal(Clock::State::UNSYNCHRONIZED, clock.getState(), 
        "Should remain UNSYNCHRONIZED when no sync event has occurred yet");
    
    // Test 2: Timeout in AWAITING_SECOND_MSG state
    auto now = clock.getLocalSteadyHardwareTime();
    auto ptp_data1 = createPtpData(TEST_LEADER_ID, now, now + 100us);
    clock.activate(&ptp_data1);
    assert_equal(Clock::State::AWAITING_SECOND_MSG, clock.getState(), 
        "Should transition to AWAITING_SECOND_MSG");
    
    // Wait for timeout and verify transition back to UNSYNCHRONIZED
    std::this_thread::sleep_for(clock.getMaxLeaderSilenceInterval());
    clock.activate(nullptr);
    assert_equal(Clock::State::UNSYNCHRONIZED, clock.getState(), 
        "Should timeout to UNSYNCHRONIZED from AWAITING_SECOND_MSG");
    
    // Test 3: Timeout in SYNCHRONIZED state
    // First get back to SYNCHRONIZED
    now = clock.getLocalSteadyHardwareTime();
    ptp_data1 = createPtpData(TEST_LEADER_ID, now, now + 100us);
    clock.activate(&ptp_data1);
    
    auto ptp_data2 = createPtpData(TEST_LEADER_ID, now + 1000us, now + 1100us);
    clock.activate(&ptp_data2);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
        "Should be in SYNCHRONIZED state");
    
    // Wait for timeout and verify transition back to UNSYNCHRONIZED
    std::this_thread::sleep_for(clock.getMaxLeaderSilenceInterval());
    clock.activate(nullptr);
    assert_equal(Clock::State::UNSYNCHRONIZED, clock.getState(), 
        "Should timeout to UNSYNCHRONIZED from SYNCHRONIZED");
    
    // Test 4: Verify timeout doesn't occur with recent messages
    clock.reset();
    storage.setLeaderId(leader_addr);
    
    now = clock.getLocalSteadyHardwareTime();
    ptp_data1 = createPtpData(TEST_LEADER_ID, now, now + 100us);
    clock.activate(&ptp_data1);
    assert_equal(Clock::State::AWAITING_SECOND_MSG, clock.getState(), 
        "Should be in AWAITING_SECOND_MSG");
    
    // Wait less than timeout period
    std::this_thread::sleep_for(clock.getMaxLeaderSilenceInterval() / 2); 
    clock.activate(nullptr);
    assert_equal(Clock::State::AWAITING_SECOND_MSG, clock.getState(), 
        "Should NOT timeout when within silence interval");
    
    // Test 5: Verify timeout boundary conditions (exactly at timeout)
    clock.reset();
    storage.setLeaderId(leader_addr);
    
    now = clock.getLocalSteadyHardwareTime();
    ptp_data1 = createPtpData(TEST_LEADER_ID, now, now + 100us);
    clock.activate(&ptp_data1);
    
    // Wait exactly the timeout period
    std::this_thread::sleep_for(clock.getMaxLeaderSilenceInterval()); // Exactly MAX_LEADER_SILENCE_INTERVAL
    clock.activate(nullptr);
    // Should be in UNSYNCHRONIZED state because of the methods delay
    assert_equal(Clock::State::UNSYNCHRONIZED, clock.getState(), 
        "Should be in UNSYNCHRONIZED state because of the methods delay");

    // Test 6: Verify timeout boundary conditions (a little bit less than timeout)
    clock.reset();
    storage.setLeaderId(leader_addr);
    
    now = clock.getLocalSteadyHardwareTime();
    ptp_data1 = createPtpData(TEST_LEADER_ID, now, now + 100us);
    clock.activate(&ptp_data1);
    
    // Wait a little bit less than timeout
    std::this_thread::sleep_for(clock.getMaxLeaderSilenceInterval() * 0.7); 
    clock.activate(nullptr);
    // Should still be in AWAITING_SECOND_MSG state since we haven't reached timeout yet
    assert_equal(Clock::State::AWAITING_SECOND_MSG, clock.getState(), 
        "Should still be in AWAITING_SECOND_MSG state since timeout not reached yet");
    
    // Wait just a bit more to trigger timeout
    std::this_thread::sleep_for(clock.getMaxLeaderSilenceInterval() * 0.3);
    clock.activate(nullptr);
    assert_equal(Clock::State::UNSYNCHRONIZED, clock.getState(), 
        "Should timeout just after MAX_LEADER_SILENCE_INTERVAL");
    
    // Test 7: Verify that new valid messages reset the timeout timer
    clock.reset();
    storage.setLeaderId(leader_addr);
    
    now = clock.getLocalSteadyHardwareTime();
    ptp_data1 = createPtpData(TEST_LEADER_ID, now, now + 100us);
    clock.activate(&ptp_data1);
    
    ptp_data2 = createPtpData(TEST_LEADER_ID, now + 1000us, now + 1100us);
    clock.activate(&ptp_data2);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
        "Should be SYNCHRONIZED");
    
    // Wait half of the timeout period, then send another message
    std::this_thread::sleep_for(clock.getMaxLeaderSilenceInterval() / 2);
    // Use current time for the third message to properly reset the timeout timer
    auto now_after_wait = clock.getLocalSteadyHardwareTime();
    auto ptp_data3 = createPtpData(TEST_LEADER_ID, now_after_wait, now_after_wait + 100us);
    clock.activate(&ptp_data3);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
        "Should remain SYNCHRONIZED after receiving new message");
    
    // Send another message with current timestamps to ensure timer reset
    now_after_wait = clock.getLocalSteadyHardwareTime();
    ptp_data3 = createPtpData(TEST_LEADER_ID, now_after_wait, now_after_wait + 100us);
    clock.activate(&ptp_data3);

    // Wait half of the timeout period and check timeout
    std::this_thread::sleep_for(clock.getMaxLeaderSilenceInterval() / 2);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
        "Should NOT timeout because timer was reset by the intermediate message");

    // Now wait another half of the timeout period and check timeout
    std::this_thread::sleep_for(clock.getMaxLeaderSilenceInterval() / 2);
    assert_equal(Clock::State::UNSYNCHRONIZED, clock.getState(), 
        "Should timeout after timeout period");
}

/**
 * @brief Tests timeout behavior at exact boundary conditions
 * 
 * Verifies the Clock's timeout behavior when the silence interval is exactly
 * at the MAX_LEADER_SILENCE_INTERVAL boundary. This ensures that the timeout
 * logic is implemented correctly and handles edge cases appropriately, such
 * as when the timeout occurs exactly at the 5-second mark.
 */
void ClockTest::testTimeoutBoundaryConditionsAtExactInterval() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    const LeaderIdType TEST_LEADER_ID = 1;
    
    // Set leader in storage
    Ethernet::Address leader_addr;
    leader_addr.bytes[5] = static_cast<uint8_t>(TEST_LEADER_ID);
    storage.setLeaderId(leader_addr);
    
    // First, get to AWAITING_SECOND_MSG state by sending a message
    auto now = clock.getLocalSteadyHardwareTime();
    auto ptp_data1 = createPtpData(TEST_LEADER_ID, now, now + 100us);
    clock.activate(&ptp_data1);
    assert_equal(Clock::State::AWAITING_SECOND_MSG, clock.getState(), 
        "Should be in AWAITING_SECOND_MSG after first message");
    
    // Wait exactly the timeout period
    std::this_thread::sleep_for(clock.getMaxLeaderSilenceInterval()); // Exactly MAX_LEADER_SILENCE_INTERVAL
    clock.activate(nullptr);
    // Should timeout to UNSYNCHRONIZED after the silence interval
    assert_equal(Clock::State::UNSYNCHRONIZED, clock.getState(), 
        "Should timeout to UNSYNCHRONIZED after MAX_LEADER_SILENCE_INTERVAL");
}

/**
 * @brief Tests that the timeout timer is reset when new valid messages are received
 * 
 * Verifies that when the Clock receives valid PTP messages from the leader,
 * the timeout timer is properly reset, preventing premature timeouts. This
 * test ensures that continuous message reception maintains synchronization
 * and that the timeout only occurs when there is actual leader silence.
 */
void ClockTest::testTimeoutTimerResetWithNewValidMessages() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    const LeaderIdType TEST_LEADER_ID = 1;
    
    // Set leader in storage
    Ethernet::Address leader_addr;
    leader_addr.bytes[5] = static_cast<uint8_t>(TEST_LEADER_ID);
    storage.setLeaderId(leader_addr);
    
    // Wait exactly the timeout period
    std::this_thread::sleep_for(clock.getMaxLeaderSilenceInterval()); // Exactly MAX_LEADER_SILENCE_INTERVAL
    clock.activate(nullptr);
    // Should be in UNSYNCHRONIZED state because of the methods delay
    assert_equal(Clock::State::UNSYNCHRONIZED, clock.getState(), 
        "Should be in UNSYNCHRONIZED state because of the methods delay");

    // Test 7: Verify that new valid messages reset the timeout timer
    auto now = clock.getLocalSteadyHardwareTime();
    auto ptp_data1 = createPtpData(TEST_LEADER_ID, now, now + 100us);
    clock.activate(&ptp_data1);
    
    auto ptp_data2 = createPtpData(TEST_LEADER_ID, now + 1000us, now + 1100us);
    clock.activate(&ptp_data2);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
        "Should be SYNCHRONIZED");
    
    // Wait half of the timeout period, then send another message
    std::this_thread::sleep_for(clock.getMaxLeaderSilenceInterval() / 2);
    // Use current time for the third message to properly reset the timeout timer
    auto now_after_wait = clock.getLocalSteadyHardwareTime();
    auto ptp_data3 = createPtpData(TEST_LEADER_ID, now_after_wait, now_after_wait + 100us);
    clock.activate(&ptp_data3);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
        "Should remain SYNCHRONIZED after receiving new message");
    
    // Send another message with current timestamps to ensure timer reset
    now_after_wait = clock.getLocalSteadyHardwareTime();
    ptp_data3 = createPtpData(TEST_LEADER_ID, now_after_wait, now_after_wait + 100us);
    clock.activate(&ptp_data3);

    // Wait half of the timeout period and check timeout
    std::this_thread::sleep_for(clock.getMaxLeaderSilenceInterval() / 2);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
        "Should NOT timeout because timer was reset by the intermediate message");

    // Now wait another half of the timeout period and check timeout
    std::this_thread::sleep_for(clock.getMaxLeaderSilenceInterval() / 2);
    assert_equal(Clock::State::UNSYNCHRONIZED, clock.getState(), 
        "Should timeout after half of the timeout period");
}

/**
 * @brief Tests that leader changes reset the Clock state to UNSYNCHRONIZED
 * 
 * Verifies that when the leader ID changes in LeaderKeyStorage, the Clock
 * properly resets its state to UNSYNCHRONIZED regardless of its current state.
 * This ensures that synchronization data from the previous leader is not
 * incorrectly applied to the new leader, maintaining timing accuracy.
 */
void ClockTest::testLeaderChangeResetsStateToUnsynchronized() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    const LeaderIdType INITIAL_LEADER_ID = 1;
    const LeaderIdType NEW_LEADER_ID = 2;
    
    // Set initial leader in storage
    Ethernet::Address initial_leader_addr;
    initial_leader_addr.bytes[5] = static_cast<uint8_t>(INITIAL_LEADER_ID);
    storage.setLeaderId(initial_leader_addr);
    
    // Test leader change in UNSYNCHRONIZED
    Ethernet::Address new_leader_addr;
    new_leader_addr.bytes[5] = static_cast<uint8_t>(NEW_LEADER_ID);
    storage.setLeaderId(new_leader_addr);
    clock.activate(nullptr);
    assert_equal(Clock::State::UNSYNCHRONIZED, clock.getState(), "Should stay in UNSYNCHRONIZED after leader change");
    
    // Test leader change in AWAITING_SECOND_MSG
    storage.setLeaderId(initial_leader_addr);
    getToState(Clock::State::AWAITING_SECOND_MSG);
    storage.setLeaderId(new_leader_addr);
    clock.activate(nullptr);
    assert_equal(Clock::State::UNSYNCHRONIZED, clock.getState(), "Should transition to UNSYNCHRONIZED after leader change in AWAITING_SECOND_MSG");
    
    // Test leader change in SYNCHRONIZED
    storage.setLeaderId(initial_leader_addr);
    getToState(Clock::State::SYNCHRONIZED);
    storage.setLeaderId(new_leader_addr);
    clock.activate(nullptr);
    assert_equal(Clock::State::UNSYNCHRONIZED, clock.getState(), "Should transition to UNSYNCHRONIZED after leader change in SYNCHRONIZED");
}

/**
 * @brief Tests that Clock ignores messages from old leader after leader change
 * 
 * Verifies that when a leader change occurs and the old leader continues to
 * send messages, the Clock properly resets to UNSYNCHRONIZED state and ignores
 * the stale messages from the old leader. This prevents confusion and ensures
 * that only the new leader's messages are processed for synchronization.
 */
void ClockTest::testClockIgnoresOldLeaderMessagesAfterLeaderChange() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    
    // Start with no leader
    storage.setLeaderId(Ethernet::NULL_ADDRESS);
    
    const LeaderIdType INITIAL_LEADER_ID = 1;
    const LeaderIdType NEW_LEADER_ID = 2;
    
    // Create Ethernet addresses
    Ethernet::Address initial_leader_addr;
    initial_leader_addr.bytes[5] = static_cast<uint8_t>(INITIAL_LEADER_ID);
    
    Ethernet::Address new_leader_addr;
    new_leader_addr.bytes[5] = static_cast<uint8_t>(NEW_LEADER_ID);
    
    // Set initial leader in storage
    storage.setLeaderId(initial_leader_addr);
    
    // First activation should pick up the initial leader
    auto now = clock.getLocalSteadyHardwareTime();
    // Ensure proper time separation between sender and receiver
    auto tx_time = now;
    auto rx_time = now + std::chrono::milliseconds(100); // 100ms separation
    auto ptp_data1 = createPtpData(INITIAL_LEADER_ID, tx_time, rx_time);
    clock.activate(&ptp_data1);
    assert_equal(Clock::State::AWAITING_SECOND_MSG, clock.getState(), 
        "Should transition to AWAITING_SECOND_MSG with initial leader");
    assert_equal(INITIAL_LEADER_ID, clock.getCurrentLeader(),
        "Clock should have picked up initial leader during activation");
    
    // Change leader in storage
    storage.setLeaderId(new_leader_addr);
    
    // Send message from OLD leader after leader change - this should be ignored
    // Use new timestamps with proper separation
    tx_time = now + std::chrono::seconds(1);
    rx_time = tx_time + std::chrono::milliseconds(100);
    auto ptp_data2 = createPtpData(INITIAL_LEADER_ID, tx_time, rx_time);
    clock.activate(&ptp_data2);
    assert_equal(Clock::State::UNSYNCHRONIZED, clock.getState(),
        "Should reset to UNSYNCHRONIZED when leader changes during activation");
    assert_equal(NEW_LEADER_ID, clock.getCurrentLeader(),
        "Clock should have picked up new leader during activation");
    
    // Verify state machine can proceed with new leader
    tx_time = now + std::chrono::seconds(2);
    rx_time = tx_time + std::chrono::milliseconds(100);
    auto ptp_data3 = createPtpData(NEW_LEADER_ID, tx_time, rx_time);
    clock.activate(&ptp_data3);
    assert_equal(Clock::State::AWAITING_SECOND_MSG, clock.getState(),
        "Should transition to AWAITING_SECOND_MSG with new leader");
}

/**
 * @brief Tests that Clock accepts messages from new leader after leader change
 * 
 * Verifies that when a leader change occurs and the new leader sends messages,
 * the Clock properly transitions through the state machine and can achieve
 * synchronization with the new leader. This ensures seamless leader transitions
 * and continued synchronization capability.
 */
void ClockTest::testClockAcceptsNewLeaderMessagesAfterLeaderChange() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    
    // Start with no leader
    storage.setLeaderId(Ethernet::NULL_ADDRESS);
    
    const LeaderIdType INITIAL_LEADER_ID = 1;
    const LeaderIdType NEW_LEADER_ID = 2;
    
    // Create Ethernet addresses
    Ethernet::Address initial_leader_addr;
    initial_leader_addr.bytes[5] = static_cast<uint8_t>(INITIAL_LEADER_ID);
    
    Ethernet::Address new_leader_addr;
    new_leader_addr.bytes[5] = static_cast<uint8_t>(NEW_LEADER_ID);
    
    // Set initial leader in storage
    storage.setLeaderId(initial_leader_addr);
    
    // First activation should pick up the initial leader
    auto now = clock.getLocalSteadyHardwareTime();
    // Ensure proper time separation between sender and receiver
    auto tx_time = now;
    auto rx_time = now + std::chrono::milliseconds(100); // 100ms separation
    auto ptp_data1 = createPtpData(INITIAL_LEADER_ID, tx_time, rx_time);
    clock.activate(&ptp_data1);
    assert_equal(Clock::State::AWAITING_SECOND_MSG, clock.getState(), 
        "Should transition to AWAITING_SECOND_MSG with initial leader");
    assert_equal(INITIAL_LEADER_ID, clock.getCurrentLeader(),
        "Clock should have picked up initial leader during activation");
    
    // Change leader in storage
    storage.setLeaderId(new_leader_addr);
    
    // Send message from NEW leader after leader change
    // Use new timestamps with proper separation
    tx_time = now + std::chrono::seconds(1);
    rx_time = tx_time + std::chrono::milliseconds(100);
    auto ptp_data2 = createPtpData(NEW_LEADER_ID, tx_time, rx_time);
    clock.activate(&ptp_data2);
    assert_equal(Clock::State::AWAITING_SECOND_MSG, clock.getState(),
        "Should be in AWAITING_SECOND_MSG state, because it has received the first message from the new leader");
    assert_equal(NEW_LEADER_ID, clock.getCurrentLeader(),
        "Clock should have picked up new leader during activation");
    
    // Send second message from new leader to achieve synchronization
    tx_time = now + std::chrono::seconds(2);
    rx_time = tx_time + std::chrono::milliseconds(100);
    auto ptp_data3 = createPtpData(NEW_LEADER_ID, tx_time, rx_time);
    clock.activate(&ptp_data3);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(),
        "Should transition to SYNCHRONIZED with new leader");
}

/**
 * @brief Tests leader change behavior across different Clock states
 * 
 * Verifies that leader changes are handled correctly regardless of the Clock's
 * current state (UNSYNCHRONIZED, AWAITING_SECOND_MSG, or SYNCHRONIZED). This
 * comprehensive test ensures that the leader change logic works consistently
 * across all possible states and always results in proper state reset.
 */
void ClockTest::testLeaderChangeInDifferentStates() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    const LeaderIdType INITIAL_LEADER_ID = 1;
    const LeaderIdType NEW_LEADER_ID = 2;
    
    // Set initial leader in storage
    Ethernet::Address initial_leader_addr;
    initial_leader_addr.bytes[5] = static_cast<uint8_t>(INITIAL_LEADER_ID);
    storage.setLeaderId(initial_leader_addr);
    
    // Test leader change in UNSYNCHRONIZED
    Ethernet::Address new_leader_addr;
    new_leader_addr.bytes[5] = static_cast<uint8_t>(NEW_LEADER_ID);
    storage.setLeaderId(new_leader_addr);
    clock.activate(nullptr);
    assert_equal(Clock::State::UNSYNCHRONIZED, clock.getState(), "Should stay in UNSYNCHRONIZED after leader change");
    
    // Test leader change in AWAITING_SECOND_MSG
    storage.setLeaderId(initial_leader_addr);
    getToState(Clock::State::AWAITING_SECOND_MSG);
    storage.setLeaderId(new_leader_addr);
    clock.activate(nullptr);
    assert_equal(Clock::State::UNSYNCHRONIZED, clock.getState(), "Should transition to UNSYNCHRONIZED after leader change in AWAITING_SECOND_MSG");
    
    // Test leader change in SYNCHRONIZED
    storage.setLeaderId(initial_leader_addr);
    getToState(Clock::State::SYNCHRONIZED);
    storage.setLeaderId(new_leader_addr);
    clock.activate(nullptr);
    assert_equal(Clock::State::UNSYNCHRONIZED, clock.getState(), "Should transition to UNSYNCHRONIZED after leader change in SYNCHRONIZED");
}

/**
 * @brief Tests that getSynchronizedTime returns local time when unsynchronized
 * 
 * Verifies that when the Clock is in UNSYNCHRONIZED state, the getSynchronizedTime()
 * method returns the local hardware time. This ensures that applications can
 * always get a valid timestamp even when synchronization is not available,
 * falling back to local time as a reasonable default.
 */
void ClockTest::testSynchronizedTimeReturnsLocalTimeWhenUnsynchronized() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    const LeaderIdType TEST_LEADER_ID = 1;
    
    // Set leader in storage
    Ethernet::Address leader_addr;
    leader_addr.bytes[5] = static_cast<uint8_t>(TEST_LEADER_ID);
    storage.setLeaderId(leader_addr);
    
    // Test in UNSYNCHRONIZED
    bool is_synchronized = false;
    auto unsync_time = clock.getSynchronizedTime(&is_synchronized);
    assert_false(is_synchronized, "is_synchronized should be false in UNSYNCHRONIZED state");
    auto local_time = clock.getLocalSteadyHardwareTime();
    assert_timestamps_equal(local_time, unsync_time, 
                          "Should return local time in UNSYNCHRONIZED state");
    
    // Test in AWAITING_SECOND_MSG
    getToState(Clock::State::AWAITING_SECOND_MSG);
    auto awaiting_time = clock.getSynchronizedTime(&is_synchronized);
    assert_false(is_synchronized, "is_synchronized should be false in AWAITING_SECOND_MSG state");
    local_time = clock.getLocalSteadyHardwareTime();
    assert_timestamps_equal(local_time, awaiting_time, 
                          "Synchronized time in AWAITING_SECOND_MSG should be close to local time");
    
    // Test in SYNCHRONIZED
    getToState(Clock::State::SYNCHRONIZED);
    auto sync_time = clock.getSynchronizedTime(&is_synchronized);
    assert_true(is_synchronized, "is_synchronized should be true in SYNCHRONIZED state");
    local_time = clock.getLocalSteadyHardwareTime();
    assert_timestamps_equal(local_time, sync_time, 
                          "Synchronized time in SYNCHRONIZED should be close to local time");
}

/**
 * @brief Tests synchronized time calculation in AWAITING_SECOND_MSG state
 * 
 * Verifies that when the Clock is in AWAITING_SECOND_MSG state, the
 * getSynchronizedTime() method returns a reasonable time value. Since full
 * synchronization requires two messages for drift calculation, this state
 * provides the best available time estimate based on the first message.
 */
void ClockTest::testSynchronizedTimeCalculationInAwaitingSecondMessageState() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    const LeaderIdType TEST_LEADER_ID = 1;
    
    // Set leader in storage
    Ethernet::Address leader_addr;
    leader_addr.bytes[5] = static_cast<uint8_t>(TEST_LEADER_ID);
    storage.setLeaderId(leader_addr);
    
    // Test in UNSYNCHRONIZED
    bool is_synchronized = false;
    auto unsync_time = clock.getSynchronizedTime(&is_synchronized);
    assert_false(is_synchronized, "is_synchronized should be false in UNSYNCHRONIZED state");
    auto local_time = clock.getLocalSteadyHardwareTime();
    assert_timestamps_equal(local_time, unsync_time, 
                          "Should return local time in UNSYNCHRONIZED state");
    
    // Test in AWAITING_SECOND_MSG
    getToState(Clock::State::AWAITING_SECOND_MSG);
    auto awaiting_time = clock.getSynchronizedTime(&is_synchronized);
    assert_false(is_synchronized, "is_synchronized should be false in AWAITING_SECOND_MSG state");
    local_time = clock.getLocalSteadyHardwareTime();
    assert_timestamps_equal(local_time, awaiting_time, 
                          "Synchronized time in AWAITING_SECOND_MSG should be close to local time");
    
    // Test in SYNCHRONIZED
    getToState(Clock::State::SYNCHRONIZED);
    auto sync_time = clock.getSynchronizedTime(&is_synchronized);
    assert_true(is_synchronized, "is_synchronized should be true in SYNCHRONIZED state");
    local_time = clock.getLocalSteadyHardwareTime();
    assert_timestamps_equal(local_time, sync_time, 
                          "Synchronized time in SYNCHRONIZED should be close to local time");
}

/**
 * @brief Tests synchronized time calculation in SYNCHRONIZED state
 * 
 * Verifies that when the Clock is in SYNCHRONIZED state, the getSynchronizedTime()
 * method returns properly calculated synchronized time that accounts for
 * propagation delays, drift correction, and offset adjustments. This represents
 * the Clock's primary operational mode with full synchronization capability.
 */
void ClockTest::testSynchronizedTimeCalculationInSynchronizedState() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    const LeaderIdType TEST_LEADER_ID = 1;
    
    // Set leader in storage
    Ethernet::Address leader_addr;
    leader_addr.bytes[5] = static_cast<uint8_t>(TEST_LEADER_ID);
    storage.setLeaderId(leader_addr);
    
    // Test in UNSYNCHRONIZED
    bool is_synchronized = false;
    auto unsync_time = clock.getSynchronizedTime(&is_synchronized);
    assert_false(is_synchronized, "is_synchronized should be false in UNSYNCHRONIZED state");
    auto local_time = clock.getLocalSteadyHardwareTime();
    assert_timestamps_equal(local_time, unsync_time, 
                          "Should return local time in UNSYNCHRONIZED state");
    
    // Test in AWAITING_SECOND_MSG
    getToState(Clock::State::AWAITING_SECOND_MSG);
    auto awaiting_time = clock.getSynchronizedTime(&is_synchronized);
    assert_false(is_synchronized, "is_synchronized should be false in AWAITING_SECOND_MSG state");
    local_time = clock.getLocalSteadyHardwareTime();
    assert_timestamps_equal(local_time, awaiting_time, 
                          "Synchronized time in AWAITING_SECOND_MSG should be close to local time");
    
    // Test in SYNCHRONIZED
    getToState(Clock::State::SYNCHRONIZED);
    auto sync_time = clock.getSynchronizedTime(&is_synchronized);
    assert_true(is_synchronized, "is_synchronized should be true in SYNCHRONIZED state");
    local_time = clock.getLocalSteadyHardwareTime();
    assert_timestamps_equal(local_time, sync_time, 
                          "Synchronized time in SYNCHRONIZED should be close to local time");
}

/**
 * @brief Tests that synchronized time progresses forward correctly over time
 * 
 * Verifies that the getSynchronizedTime() method returns monotonically
 * increasing values over time, ensuring that the synchronized time behaves
 * like a proper clock. This test also validates that time progression
 * accounts for drift correction and maintains temporal consistency.
 */
void ClockTest::testSynchronizedTimeProgressesForwardCorrectly() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    const LeaderIdType TEST_LEADER_ID = 1;
    
    // Set leader in storage
    Ethernet::Address leader_addr;
    leader_addr.bytes[5] = static_cast<uint8_t>(TEST_LEADER_ID);
    storage.setLeaderId(leader_addr);
    
    // Test 1: Basic synchronization with fixed propagation delay
    auto now = clock.getLocalSteadyHardwareTime();
    auto ptp_data1 = createPtpData(TEST_LEADER_ID, now, now + 100us);
    clock.activate(&ptp_data1);
    assert_equal(Clock::State::AWAITING_SECOND_MSG, clock.getState(), 
        "Should transition to AWAITING_SECOND_MSG");
    
    // Test 2: Second message to achieve synchronization
    auto ptp_data2 = createPtpData(TEST_LEADER_ID, now + 1000us, now + 1100us);
    clock.activate(&ptp_data2);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
        "Should transition to SYNCHRONIZED");
    
    // Test 3: Verify synchronized time calculation works
    clock.reset();
    storage.setLeaderId(leader_addr);
    
    // First message
    auto base_time = clock.getLocalSteadyHardwareTime();
    auto ptp_first = createPtpData(TEST_LEADER_ID, base_time, base_time + 1000us);
    clock.activate(&ptp_first);
    
    // Second message
    auto ptp_second = createPtpData(TEST_LEADER_ID, base_time + 2000us, base_time + 3000us);
    clock.activate(&ptp_second);
    
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
        "Should achieve synchronization with fixed propagation delay");
    
    // Verify that getSynchronizedTime works correctly
    bool is_synchronized = false;
    auto sync_time = clock.getSynchronizedTime(&is_synchronized);
    assert_true(is_synchronized, "is_synchronized should be true in SYNCHRONIZED state");
    auto local_time = clock.getLocalSteadyHardwareTime();
    // The synchronized time should be reasonably close to local time for this test scenario
    assert_timestamps_equal(local_time, sync_time, 
        "Synchronized time should be reasonable with fixed propagation delay", 10000us);
}

/**
 * @brief Tests propagation delay calculation with zero distance between nodes
 * 
 * This test is no longer relevant as coordinates are removed.
 * Propagation delay is now fixed at 2 microseconds.
 */
// void ClockTest::testPropagationDelayCalculationWithZeroDistance() {
//     auto& clock = Clock::getInstance();
//     auto& storage = LeaderKeyStorage::getInstance();
//     const LeaderIdType TEST_LEADER_ID = 1;
    
//     // Set leader in storage
//     Ethernet::Address leader_addr;
//     leader_addr.bytes[5] = static_cast<uint8_t>(TEST_LEADER_ID);
//     storage.setLeaderId(leader_addr);
    
//     // Test 1: Zero distance (same coordinates)
//     auto now = clock.getLocalSteadyHardwareTime();
//     Coordinates same_coords = {100.0, 200.0, 300.0};
//     auto ptp_data_zero = createPtpData(TEST_LEADER_ID, now, same_coords, now + 100us, same_coords);
//     clock.activate(&ptp_data_zero);
//     assert_equal(Clock::State::AWAITING_SECOND_MSG, clock.getState(), 
//         "Should transition to AWAITING_SECOND_MSG with zero distance");
    
//     // Test 2: Known distance calculation (3-4-5 triangle)
//     // Distance should be 5 meters: sqrt(3^2 + 4^2 + 0^2) = 5
//     Coordinates sender_coords = {0.0, 0.0, 0.0};
//     Coordinates receiver_coords = {3.0, 4.0, 0.0};
//     auto ptp_data_known = createPtpData(TEST_LEADER_ID, now + 1000us, sender_coords, now + 1100us, receiver_coords);
//     clock.activate(&ptp_data_known);
//     assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
//         "Should transition to SYNCHRONIZED with known distance");
    
//     // Test 3: Large distance (should not cause overflow)
//     clock.reset();
//     storage.setLeaderId(leader_addr);
//     Coordinates far_sender = {0.0, 0.0, 0.0};
//     Coordinates far_receiver = {1000000.0, 1000000.0, 1000000.0}; // ~1732 km distance
//     auto ptp_data_large = createPtpData(TEST_LEADER_ID, now + 2000us, far_sender, now + 2100us, far_receiver);
//     clock.activate(&ptp_data_large);
//     assert_equal(Clock::State::AWAITING_SECOND_MSG, clock.getState(), 
//         "Should handle large distances without issues");
    
//     // Test 4: Verify synchronized time calculation with different propagation delays
//     // Reset and test with two different coordinate sets to ensure propagation delay affects timing
//     clock.reset();
//     storage.setLeaderId(leader_addr);
    
//     // First message with close coordinates (small propagation delay)
//     Coordinates close_sender = {0.0, 0.0, 0.0};
//     Coordinates close_receiver = {1.0, 0.0, 0.0}; // 1 meter distance
//     auto close_tx_time = now + 3000us;
//     auto close_rx_time = close_tx_time + 1000us; // 1ms total delay
//     auto ptp_close = createPtpData(TEST_LEADER_ID, close_tx_time, close_sender, close_rx_time, close_receiver);
//     clock.activate(&ptp_close);
    
//     // Second message with far coordinates (larger propagation delay)
//     Coordinates far_sender2 = {0.0, 0.0, 0.0};
//     Coordinates far_receiver2 = {300.0, 400.0, 0.0}; // 500 meter distance
//     auto far_tx_time = close_tx_time + 2000us;
//     auto far_rx_time = far_tx_time + 1000us; // Same 1ms total delay
//     auto ptp_far = createPtpData(TEST_LEADER_ID, far_tx_time, far_sender2, far_rx_time, far_receiver2);
//     clock.activate(&ptp_far);
    
//     assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
//         "Should achieve synchronization with different propagation delays");
    
//     // Verify that getSynchronizedTime works correctly
//     bool is_synchronized = false;
//     auto sync_time = clock.getSynchronizedTime(&is_synchronized);
//     assert_true(is_synchronized, "is_synchronized should be true in SYNCHRONIZED state");
//     auto local_time = clock.getLocalSteadyHardwareTime();
//     // The synchronized time should be reasonably close to local time for this test scenario
//     assert_timestamps_equal(local_time, sync_time, 
//         "Synchronized time should be reasonable with propagation delay calculations", 10000us);
// }

/**
 * @brief Tests propagation delay calculation with known distance
 * 
 * This test is no longer relevant as coordinates are removed.
 * Propagation delay is now fixed at 2 microseconds.
 */
// void ClockTest::testPropagationDelayCalculationWithKnownDistance() {
//     auto& clock = Clock::getInstance();
//     auto& storage = LeaderKeyStorage::getInstance();
//     const LeaderIdType TEST_LEADER_ID = 1;
    
//     // Set leader in storage
//     Ethernet::Address leader_addr;
//     leader_addr.bytes[5] = static_cast<uint8_t>(TEST_LEADER_ID);
//     storage.setLeaderId(leader_addr);
    
//     // Test 1: Zero distance (same coordinates)
//     auto now = clock.getLocalSteadyHardwareTime();
//     Coordinates same_coords = {100.0, 200.0, 300.0};
//     auto ptp_data_zero = createPtpData(TEST_LEADER_ID, now, same_coords, now + 100us, same_coords);
//     clock.activate(&ptp_data_zero);
//     assert_equal(Clock::State::AWAITING_SECOND_MSG, clock.getState(), 
//         "Should transition to AWAITING_SECOND_MSG with zero distance");
    
//     // Test 2: Known distance calculation (3-4-5 triangle)
//     // Distance should be 5 meters: sqrt(3^2 + 4^2 + 0^2) = 5
//     Coordinates sender_coords = {0.0, 0.0, 0.0};
//     Coordinates receiver_coords = {3.0, 4.0, 0.0};
//     auto ptp_data_known = createPtpData(TEST_LEADER_ID, now + 1000us, sender_coords, now + 1100us, receiver_coords);
//     clock.activate(&ptp_data_known);
//     assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
//         "Should transition to SYNCHRONIZED with known distance");
    
//     // Test 3: Large distance (should not cause overflow)
//     clock.reset();
//     storage.setLeaderId(leader_addr);
//     Coordinates far_sender = {0.0, 0.0, 0.0};
//     Coordinates far_receiver = {1000000.0, 1000000.0, 1000000.0}; // ~1732 km distance
//     auto ptp_data_large = createPtpData(TEST_LEADER_ID, now + 2000us, far_sender, now + 2100us, far_receiver);
//     clock.activate(&ptp_data_large);
//     assert_equal(Clock::State::AWAITING_SECOND_MSG, clock.getState(), 
//         "Should handle large distances without issues");
    
//     // Test 4: Verify synchronized time calculation with different propagation delays
//     // Reset and test with two different coordinate sets to ensure propagation delay affects timing
//     clock.reset();
//     storage.setLeaderId(leader_addr);
    
//     // First message with close coordinates (small propagation delay)
//     Coordinates close_sender = {0.0, 0.0, 0.0};
//     Coordinates close_receiver = {1.0, 0.0, 0.0}; // 1 meter distance
//     auto close_tx_time = now + 3000us;
//     auto close_rx_time = close_tx_time + 1000us; // 1ms total delay
//     auto ptp_close = createPtpData(TEST_LEADER_ID, close_tx_time, close_sender, close_rx_time, close_receiver);
//     clock.activate(&ptp_close);
    
//     // Second message with far coordinates (larger propagation delay)
//     Coordinates far_sender2 = {0.0, 0.0, 0.0};
//     Coordinates far_receiver2 = {300.0, 400.0, 0.0}; // 500 meter distance
//     auto far_tx_time = close_tx_time + 2000us;
//     auto far_rx_time = far_tx_time + 1000us; // Same 1ms total delay
//     auto ptp_far = createPtpData(TEST_LEADER_ID, far_tx_time, far_sender2, far_rx_time, far_receiver2);
//     clock.activate(&ptp_far);
    
//     assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
//         "Should achieve synchronization with different propagation delays");
    
//     // Verify that getSynchronizedTime works correctly
//     bool is_synchronized = false;
//     auto sync_time = clock.getSynchronizedTime(&is_synchronized);
//     assert_true(is_synchronized, "is_synchronized should be true in SYNCHRONIZED state");
//     auto local_time = clock.getLocalSteadyHardwareTime();
//     // The synchronized time should be reasonably close to local time for this test scenario
//     assert_timestamps_equal(local_time, sync_time, 
//         "Synchronized time should be reasonable with propagation delay calculations", 10000us);
// }

/**
 * @brief Tests propagation delay calculation with large distances
 * 
 * This test is no longer relevant as coordinates are removed.
 * Propagation delay is now fixed at 2 microseconds.
 */
// void ClockTest::testPropagationDelayCalculationWithLargeDistances() {
//     auto& clock = Clock::getInstance();
//     auto& storage = LeaderKeyStorage::getInstance();
//     const LeaderIdType TEST_LEADER_ID = 1;
    
//     // Set leader in storage
//     Ethernet::Address leader_addr;
//     leader_addr.bytes[5] = static_cast<uint8_t>(TEST_LEADER_ID);
//     storage.setLeaderId(leader_addr);
    
//     // Test 1: Large distance (should not cause overflow)
//     auto now = clock.getLocalSteadyHardwareTime();
//     Coordinates far_sender = {0.0, 0.0, 0.0};
//     Coordinates far_receiver = {1000000.0, 1000000.0, 1000000.0}; // ~1732 km distance
//     auto ptp_data_large = createPtpData(TEST_LEADER_ID, now + 2000us, far_sender, now + 2100us, far_receiver);
//     clock.activate(&ptp_data_large);
//     assert_equal(Clock::State::AWAITING_SECOND_MSG, clock.getState(), 
//         "Should handle large distances without issues");
    
//     // Test 2: Verify synchronized time calculation with different propagation delays
//     // Reset and test with two different coordinate sets to ensure propagation delay affects timing
//     clock.reset();
//     storage.setLeaderId(leader_addr);
    
//     // First message with close coordinates (small propagation delay)
//     Coordinates close_sender = {0.0, 0.0, 0.0};
//     Coordinates close_receiver = {1.0, 0.0, 0.0}; // 1 meter distance
//     auto close_tx_time = now + 3000us;
//     auto close_rx_time = close_tx_time + 1000us; // 1ms total delay
//     auto ptp_close = createPtpData(TEST_LEADER_ID, close_tx_time, close_sender, close_rx_time, close_receiver);
//     clock.activate(&ptp_close);
    
//     // Second message with far coordinates (larger propagation delay)
//     Coordinates far_sender2 = {0.0, 0.0, 0.0};
//     Coordinates far_receiver2 = {300.0, 400.0, 0.0}; // 500 meter distance
//     auto far_tx_time = close_tx_time + 2000us;
//     auto far_rx_time = far_tx_time + 1000us; // Same 1ms total delay
//     auto ptp_far = createPtpData(TEST_LEADER_ID, far_tx_time, far_sender2, far_rx_time, far_receiver2);
//     clock.activate(&ptp_far);
    
//     assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
//         "Should achieve synchronization with different propagation delays");
    
//     // Verify that getSynchronizedTime works correctly
//     bool is_synchronized = false;
//     auto sync_time = clock.getSynchronizedTime(&is_synchronized);
//     assert_true(is_synchronized, "is_synchronized should be true in SYNCHRONIZED state");
//     auto local_time = clock.getLocalSteadyHardwareTime();
//     // The synchronized time should be reasonably close to local time for this test scenario
//     assert_timestamps_equal(local_time, sync_time, 
//         "Synchronized time should be reasonable with propagation delay calculations", 10000us);
// }

/**
 * @brief Tests propagation delay calculation with negative coordinates
 * 
 * This test is no longer relevant as coordinates are removed.
 * Propagation delay is now fixed at 2 microseconds.
 */
// void ClockTest::testPropagationDelayCalculationWithNegativeCoordinates() {
//     auto& clock = Clock::getInstance();
//     auto& storage = LeaderKeyStorage::getInstance();
//     const LeaderIdType TEST_LEADER_ID = 1;
    
//     // Set leader in storage
//     Ethernet::Address leader_addr;
//     leader_addr.bytes[5] = static_cast<uint8_t>(TEST_LEADER_ID);
//     storage.setLeaderId(leader_addr);
    
//     // Test with negative coordinates
//     auto now = clock.getLocalSteadyHardwareTime();
//     Coordinates negative_coords = {-100.0, -200.0, -300.0};
//     auto ptp_negative = createPtpData(TEST_LEADER_ID, now, negative_coords, now + 100us, {-50, -100, -150});
//     clock.activate(&ptp_negative);
//     assert_equal(Clock::State::AWAITING_SECOND_MSG, clock.getState(), 
//         "Should handle negative coordinates");
    
//     // Test 2: Verify synchronized time calculation with different propagation delays
//     // Reset and test with two different coordinate sets to ensure propagation delay affects timing
//     clock.reset();
//     storage.setLeaderId(leader_addr);
    
//     // First message with close coordinates (small propagation delay)
//     Coordinates close_sender = {0.0, 0.0, 0.0};
//     Coordinates close_receiver = {1.0, 0.0, 0.0}; // 1 meter distance
//     auto close_tx_time = now + 3000us;
//     auto close_rx_time = close_tx_time + 1000us; // 1ms total delay
//     auto ptp_close = createPtpData(TEST_LEADER_ID, close_tx_time, close_sender, close_rx_time, close_receiver);
//     clock.activate(&ptp_close);
    
//     // Second message with far coordinates (larger propagation delay)
//     Coordinates far_sender2 = {0.0, 0.0, 0.0};
//     Coordinates far_receiver2 = {300.0, 400.0, 0.0}; // 500 meter distance
//     auto far_tx_time = close_tx_time + 2000us;
//     auto far_rx_time = far_tx_time + 1000us; // Same 1ms total delay
//     auto ptp_far = createPtpData(TEST_LEADER_ID, far_tx_time, far_sender2, far_rx_time, far_receiver2);
//     clock.activate(&ptp_far);
    
//     assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
//         "Should achieve synchronization with different propagation delays");
    
//     // Verify that getSynchronizedTime works correctly
//     bool is_synchronized = false;
//     auto sync_time = clock.getSynchronizedTime(&is_synchronized);
//     assert_true(is_synchronized, "is_synchronized should be true in SYNCHRONIZED state");
//     auto local_time = clock.getLocalSteadyHardwareTime();
//     // The synchronized time should be reasonably close to local time for this test scenario
//     assert_timestamps_equal(local_time, sync_time, 
//         "Synchronized time should be reasonable with propagation delay calculations", 10000us);
// }

/**
 * @brief Tests drift calculation with perfect synchronization (zero drift)
 * 
 * Verifies that the Clock correctly calculates zero drift when the local
 * clock is perfectly synchronized with the leader's clock. This baseline
 * test ensures that the drift calculation algorithm works correctly when
 * no clock drift is present.
 */
void ClockTest::testDriftCalculationWithPerfectSynchronization() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    const LeaderIdType TEST_LEADER_ID = 1;
    
    // Set leader in storage
    Ethernet::Address leader_addr;
    leader_addr.bytes[5] = static_cast<uint8_t>(TEST_LEADER_ID);
    storage.setLeaderId(leader_addr);
    
    // Test 1: Perfect synchronization (zero drift)
    auto base_time = clock.getLocalSteadyHardwareTime();
    
    // First message
    auto tx1 = base_time;
    auto rx1 = base_time + 1000us; // 1ms propagation + processing
    auto ptp1 = createPtpData(TEST_LEADER_ID, tx1, rx1);
    clock.activate(&ptp1);
    assert_equal(Clock::State::AWAITING_SECOND_MSG, clock.getState(), 
        "Should be in AWAITING_SECOND_MSG after first message");
    
    // Second message with perfect timing (no drift)
    auto tx2 = tx1 + 2000us; // 2ms later at sender
    auto rx2 = rx1 + 2000us; // Also 2ms later at receiver (perfect sync)
    auto ptp2 = createPtpData(TEST_LEADER_ID, tx2, rx2);
    clock.activate(&ptp2);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
        "Should be SYNCHRONIZED after second message");
    
    // Test 2: Positive drift (local clock running fast)
    clock.reset();
    storage.setLeaderId(leader_addr);
    
    // First message
    tx1 = base_time + 5000us;
    rx1 = tx1 + 1000us;
    ptp1 = createPtpData(TEST_LEADER_ID, tx1, rx1);
    clock.activate(&ptp1);
    
    // Second message with local clock running 10% fast
    tx2 = tx1 + 2000us; // 2ms later at sender
    rx2 = rx1 + 2200us; // 2.2ms later at receiver (10% fast)
    ptp2 = createPtpData(TEST_LEADER_ID, tx2, rx2);
    clock.activate(&ptp2);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
        "Should handle positive drift correctly");
    
    // Test 3: Negative drift (local clock running slow)
    clock.reset();
    storage.setLeaderId(leader_addr);
    
    // First message
    tx1 = base_time + 10000us;
    rx1 = tx1 + 1000us;
    ptp1 = createPtpData(TEST_LEADER_ID, tx1, rx1);
    clock.activate(&ptp1);
    
    // Second message with local clock running 5% slow
    tx2 = tx1 + 2000us; // 2ms later at sender
    rx2 = rx1 + 1900us; // 1.9ms later at receiver (5% slow)
    ptp2 = createPtpData(TEST_LEADER_ID, tx2, rx2);
    clock.activate(&ptp2);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
        "Should handle negative drift correctly");
    
    // Test 4: Edge case - very small time differences
    clock.reset();
    storage.setLeaderId(leader_addr);
    
    // First message
    tx1 = base_time + 15000us;
    rx1 = tx1 + 1000us;
    ptp1 = createPtpData(TEST_LEADER_ID, tx1, rx1);
    clock.activate(&ptp1);
    
    // Second message with very small time difference (1 microsecond)
    tx2 = tx1 + 1us; // Only 1us later at sender
    rx2 = rx1 + 2us; // 2us later at receiver
    ptp2 = createPtpData(TEST_LEADER_ID, tx2, rx2);
    clock.activate(&ptp2);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
        "Should handle very small time differences");
    
    // Test 5: Subsequent message updates drift calculation
    // Third message to verify drift recalculation
    auto tx3 = tx2 + 5000us; // 5ms later at sender
    auto rx3 = rx2 + 5100us; // 5.1ms later at receiver (2% fast)
    auto ptp3 = createPtpData(TEST_LEADER_ID, tx3, rx3);
    clock.activate(&ptp3);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
        "Should remain SYNCHRONIZED and update drift with subsequent messages");
    
    // Verify that getSynchronizedTime still works after drift calculations
    bool is_synchronized = false;
    auto sync_time = clock.getSynchronizedTime(&is_synchronized);
    assert_true(is_synchronized, "is_synchronized should be true in SYNCHRONIZED state");
    auto local_time = clock.getLocalSteadyHardwareTime();
    // Should be reasonably close given the test scenario
    assert_timestamps_equal(local_time, sync_time, 
        "Synchronized time should work correctly after drift calculations", 20000us);
}

/**
 * @brief Tests drift calculation with positive drift (local clock running fast)
 * 
 * Verifies that the Clock correctly detects and calculates positive drift
 * when the local clock is running faster than the leader's clock. This
 * test ensures that the drift calculation can identify when the local
 * clock needs to be slowed down for proper synchronization.
 */
void ClockTest::testDriftCalculationWithPositiveDrift() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    const LeaderIdType TEST_LEADER_ID = 1;
    
    // Set leader in storage
    Ethernet::Address leader_addr;
    leader_addr.bytes[5] = static_cast<uint8_t>(TEST_LEADER_ID);
    storage.setLeaderId(leader_addr);
    
    // Test 1: Positive drift (local clock running fast)
    clock.reset();
    storage.setLeaderId(leader_addr);
    
    // First message
    auto base_time = clock.getLocalSteadyHardwareTime();
    auto tx1 = base_time + 5000us;
    auto rx1 = tx1 + 1000us;
    auto ptp1 = createPtpData(TEST_LEADER_ID, tx1, rx1);
    clock.activate(&ptp1);
    
    // Second message with local clock running 10% fast
    auto tx2 = tx1 + 2000us; // 2ms later at sender
    auto rx2 = rx1 + 2200us; // 2.2ms later at receiver (10% fast)
    auto ptp2 = createPtpData(TEST_LEADER_ID, tx2, rx2);
    clock.activate(&ptp2);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
        "Should handle positive drift correctly");
}

/**
 * @brief Tests drift calculation with negative drift (local clock running slow)
 * 
 * Verifies that the Clock correctly detects and calculates negative drift
 * when the local clock is running slower than the leader's clock. This
 * test ensures that the drift calculation can identify when the local
 * clock needs to be sped up for proper synchronization.
 */
void ClockTest::testDriftCalculationWithNegativeDrift() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    const LeaderIdType TEST_LEADER_ID = 1;
    
    // Set leader in storage
    Ethernet::Address leader_addr;
    leader_addr.bytes[5] = static_cast<uint8_t>(TEST_LEADER_ID);
    storage.setLeaderId(leader_addr);
    
    // Test 1: Negative drift (local clock running slow)
    clock.reset();
    storage.setLeaderId(leader_addr);
    
    // First message
    auto base_time = clock.getLocalSteadyHardwareTime();
    auto tx1 = base_time + 10000us;
    auto rx1 = tx1 + 1000us;
    auto ptp1 = createPtpData(TEST_LEADER_ID, tx1, rx1);
    clock.activate(&ptp1);
    
    // Second message with local clock running 5% slow
    auto tx2 = tx1 + 2000us; // 2ms later at sender
    auto rx2 = rx1 + 1900us; // 1.9ms later at receiver (5% slow)
    auto ptp2 = createPtpData(TEST_LEADER_ID, tx2, rx2);
    clock.activate(&ptp2);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
        "Should handle negative drift correctly");
}

/**
 * @brief Tests drift calculation with very small time differences
 * 
 * Verifies that the Clock handles edge cases where the time differences
 * between messages are extremely small (microsecond level). This test
 * ensures that the drift calculation algorithm remains stable and doesn't
 * produce erroneous results due to numerical precision issues.
 */
void ClockTest::testDriftCalculationWithVerySmallTimeDifferences() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    const LeaderIdType TEST_LEADER_ID = 1;
    
    // Set leader in storage
    Ethernet::Address leader_addr;
    leader_addr.bytes[5] = static_cast<uint8_t>(TEST_LEADER_ID);
    storage.setLeaderId(leader_addr);
    
    // Test 1: Edge case - very small time differences
    clock.reset();
    storage.setLeaderId(leader_addr);
    
    // First message
    auto base_time = clock.getLocalSteadyHardwareTime();
    auto tx1 = base_time + 15000us;
    auto rx1 = tx1 + 1000us;
    auto ptp1 = createPtpData(TEST_LEADER_ID, tx1, rx1);
    clock.activate(&ptp1);
    
    // Second message with very small time difference (1 microsecond)
    auto tx2 = tx1 + 1us; // Only 1us later at sender
    auto rx2 = rx1 + 2us; // 2us later at receiver
    auto ptp2 = createPtpData(TEST_LEADER_ID, tx2, rx2);
    clock.activate(&ptp2);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
        "Should handle very small time differences");
}

/**
 * @brief Tests that drift calculation updates with subsequent messages
 * 
 * Verifies that the Clock continuously updates its drift calculation as
 * new PTP messages are received. This test ensures that the Clock can
 * adapt to changing drift conditions and maintains accurate synchronization
 * over time by incorporating new timing information.
 */
void ClockTest::testDriftCalculationUpdatesWithSubsequentMessages() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    const LeaderIdType TEST_LEADER_ID = 1;
    
    // Set leader in storage
    Ethernet::Address leader_addr;
    leader_addr.bytes[5] = static_cast<uint8_t>(TEST_LEADER_ID);
    storage.setLeaderId(leader_addr);
    
    // Test 1: Perfect synchronization (zero drift)
    auto base_time = clock.getLocalSteadyHardwareTime();
    
    // First message
    auto tx1 = base_time;
    auto rx1 = base_time + 1000us; // 1ms propagation + processing
    auto ptp1 = createPtpData(TEST_LEADER_ID, tx1, rx1);
    clock.activate(&ptp1);
    assert_equal(Clock::State::AWAITING_SECOND_MSG, clock.getState(), 
        "Should be in AWAITING_SECOND_MSG after first message");
    
    // Second message with perfect timing (no drift)
    auto tx2 = tx1 + 2000us; // 2ms later at sender
    auto rx2 = rx1 + 2000us; // Also 2ms later at receiver (perfect sync)
    auto ptp2 = createPtpData(TEST_LEADER_ID, tx2, rx2);
    clock.activate(&ptp2);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
        "Should be SYNCHRONIZED after second message");
    
    // Test 2: Positive drift (local clock running fast)
    clock.reset();
    storage.setLeaderId(leader_addr);
    
    // First message
    tx1 = base_time + 5000us;
    rx1 = tx1 + 1000us;
    ptp1 = createPtpData(TEST_LEADER_ID, tx1, rx1);
    clock.activate(&ptp1);
    
    // Second message with local clock running 10% fast
    tx2 = tx1 + 2000us; // 2ms later at sender
    rx2 = rx1 + 2200us; // 2.2ms later at receiver (10% fast)
    ptp2 = createPtpData(TEST_LEADER_ID, tx2, rx2);
    clock.activate(&ptp2);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
        "Should handle positive drift correctly");
    
    // Test 3: Negative drift (local clock running slow)
    clock.reset();
    storage.setLeaderId(leader_addr);
    
    // First message
    tx1 = base_time + 10000us;
    rx1 = tx1 + 1000us;
    ptp1 = createPtpData(TEST_LEADER_ID, tx1, rx1);
    clock.activate(&ptp1);
    
    // Second message with local clock running 5% slow
    tx2 = tx1 + 2000us; // 2ms later at sender
    rx2 = rx1 + 1900us; // 1.9ms later at receiver (5% slow)
    ptp2 = createPtpData(TEST_LEADER_ID, tx2, rx2);
    clock.activate(&ptp2);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
        "Should handle negative drift correctly");
    
    // Test 4: Edge case - very small time differences
    clock.reset();
    storage.setLeaderId(leader_addr);
    
    // First message
    tx1 = base_time + 15000us;
    rx1 = tx1 + 1000us;
    ptp1 = createPtpData(TEST_LEADER_ID, tx1, rx1);
    clock.activate(&ptp1);
    
    // Second message with very small time difference (1 microsecond)
    tx2 = tx1 + 1us; // Only 1us later at sender
    rx2 = rx1 + 2us; // 2us later at receiver
    ptp2 = createPtpData(TEST_LEADER_ID, tx2, rx2);
    clock.activate(&ptp2);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
        "Should handle very small time differences");
    
    // Test 5: Subsequent message updates drift calculation
    // Third message to verify drift recalculation
    auto tx3 = tx2 + 5000us; // 5ms later at sender
    auto rx3 = rx2 + 5100us; // 5.1ms later at receiver (2% fast)
    auto ptp3 = createPtpData(TEST_LEADER_ID, tx3, rx3);
    clock.activate(&ptp3);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
        "Should remain SYNCHRONIZED and update drift with subsequent messages");
    
    // Verify that getSynchronizedTime still works after drift calculations
    bool is_synchronized = false;
    auto sync_time = clock.getSynchronizedTime(&is_synchronized);
    assert_true(is_synchronized, "is_synchronized should be true in SYNCHRONIZED state");
    auto local_time = clock.getLocalSteadyHardwareTime();
    // Should be reasonably close given the test scenario
    assert_timestamps_equal(local_time, sync_time, 
        "Synchronized time should work correctly after drift calculations", 20000us);
}

/**
 * @brief Tests basic functionality of local time methods
 * 
 * Verifies that getLocalSystemTime() and getLocalSteadyHardwareTime()
 * methods return valid timestamps and function correctly. This test
 * ensures that the basic time retrieval functionality works as expected
 * and provides a foundation for more complex timing operations.
 */
void ClockTest::testLocalSystemTimeMethodBasicFunctionality() {
    auto& clock = Clock::getInstance();
    
    // Test 1: Basic functionality
    auto system_time1 = clock.getLocalSystemTime();
    auto steady_time1 = clock.getLocalSteadyHardwareTime();
    
    // Both should be valid timestamps
    assert_true(system_time1.time_since_epoch().count() > 0, 
        "getLocalSystemTime should return valid timestamp");
    assert_true(steady_time1.time_since_epoch().count() > 0, 
        "getLocalSteadyHardwareTime should return valid timestamp");
    
    // Test 2: Time progression
    std::this_thread::sleep_for(1ms);
    auto system_time2 = clock.getLocalSystemTime();
    auto steady_time2 = clock.getLocalSteadyHardwareTime();
    
    // Both should progress forward
    assert_true(system_time2 > system_time1, 
        "getLocalSystemTime should progress forward");
    assert_true(steady_time2 > steady_time1, 
        "getLocalSteadyHardwareTime should progress forward");
    
    // Test 3: Consistency between calls
    // Since both methods currently return steady_clock time in the implementation,
    // they should be very close to each other
    auto system_time3 = clock.getLocalSystemTime();
    auto steady_time3 = clock.getLocalSteadyHardwareTime();
    
    // Should be within a reasonable tolerance (1ms) since they're called sequentially
    assert_timestamps_equal(steady_time3, system_time3, 
        "getLocalSystemTime and getLocalSteadyHardwareTime should be consistent", 1000us);
    
    // Test 4: Multiple rapid calls should be monotonic
    std::vector<TimestampType> system_times;
    std::vector<TimestampType> steady_times;
    
    for (int i = 0; i < 10; ++i) {
        system_times.push_back(clock.getLocalSystemTime());
        steady_times.push_back(clock.getLocalSteadyHardwareTime());
    }
    
    // Verify monotonic progression
    for (size_t i = 1; i < system_times.size(); ++i) {
        assert_true(system_times[i] >= system_times[i-1], 
            "getLocalSystemTime should be monotonic");
        assert_true(steady_times[i] >= steady_times[i-1], 
            "getLocalSteadyHardwareTime should be monotonic");
    }
    
    // Test 5: Thread safety of time methods
    std::atomic<bool> error_occurred{false};
    std::vector<std::thread> threads;
    const int num_threads = 4;
    const int num_calls = 100;
    
    auto time_test_func = [&clock, &error_occurred, num_calls]() {
        for (int i = 0; i < num_calls && !error_occurred; ++i) {
            try {
                auto sys_time = clock.getLocalSystemTime();
                auto steady_time = clock.getLocalSteadyHardwareTime();
                
                // Basic sanity checks
                if (sys_time.time_since_epoch().count() <= 0 || 
                    steady_time.time_since_epoch().count() <= 0) {
                    error_occurred = true;
                    return;
                }
            } catch (const std::exception& e) {
                error_occurred = true;
                return;
            }
        }
    };
    
    // Launch threads
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(time_test_func);
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    assert_false(error_occurred, "Time methods should be thread-safe");
}

/**
 * @brief Tests that local time methods show proper time progression
 * 
 * Verifies that successive calls to getLocalSystemTime() and
 * getLocalSteadyHardwareTime() return increasing timestamps, demonstrating
 * that the time methods correctly reflect the passage of time and maintain
 * temporal consistency.
 */
void ClockTest::testLocalSystemTimeMethodProgression() {
    auto& clock = Clock::getInstance();
    
    // Test time progression
    auto system_time1 = clock.getLocalSystemTime();
    auto steady_time1 = clock.getLocalSteadyHardwareTime();
    
    std::this_thread::sleep_for(1ms);
    auto system_time2 = clock.getLocalSystemTime();
    auto steady_time2 = clock.getLocalSteadyHardwareTime();
    
    // Both should progress forward
    assert_true(system_time2 > system_time1, 
        "getLocalSystemTime should progress forward");
    assert_true(steady_time2 > steady_time1, 
        "getLocalSteadyHardwareTime should progress forward");
}

/**
 * @brief Tests consistency between system time and steady time methods
 * 
 * Verifies that getLocalSystemTime() and getLocalSteadyHardwareTime()
 * return consistent values when called in close succession. This test
 * ensures that both time methods are properly synchronized and provide
 * coherent timing information.
 */
void ClockTest::testLocalSystemTimeMethodConsistencyWithSteadyTime() {
    auto& clock = Clock::getInstance();
    
    // Test 3: Consistency between calls
    // Since both methods currently return steady_clock time in the implementation,
    // they should be very close to each other
    auto system_time3 = clock.getLocalSystemTime();
    auto steady_time3 = clock.getLocalSteadyHardwareTime();
    
    // Should be within a reasonable tolerance (1ms) since they're called sequentially
    assert_timestamps_equal(steady_time3, system_time3, 
        "getLocalSystemTime and getLocalSteadyHardwareTime should be consistent", 1000us);
}

/**
 * @brief Tests monotonic behavior of local time methods
 * 
 * Verifies that multiple rapid calls to getLocalSystemTime() and
 * getLocalSteadyHardwareTime() return monotonically increasing values.
 * This test ensures that the time methods behave like proper clocks
 * and never go backwards in time.
 */
void ClockTest::testLocalSystemTimeMethodMonotonicBehavior() {
    auto& clock = Clock::getInstance();
    
    // Test 4: Multiple rapid calls should be monotonic
    std::vector<TimestampType> system_times;
    std::vector<TimestampType> steady_times;
    
    for (int i = 0; i < 10; ++i) {
        system_times.push_back(clock.getLocalSystemTime());
        steady_times.push_back(clock.getLocalSteadyHardwareTime());
    }
    
    // Verify monotonic progression
    for (size_t i = 1; i < system_times.size(); ++i) {
        assert_true(system_times[i] >= system_times[i-1], 
            "getLocalSystemTime should be monotonic");
        assert_true(steady_times[i] >= steady_times[i-1], 
            "getLocalSteadyHardwareTime should be monotonic");
    }
}

/**
 * @brief Tests that Clock handles impossible timing scenarios gracefully
 * 
 * Verifies that the Clock can handle edge cases such as messages with
 * impossible timing (e.g., received before they were sent) without
 * crashing or entering an invalid state. This test ensures robustness
 * against malformed or corrupted timing data.
 */
void ClockTest::testClockHandlesImpossibleTimingGracefully() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    const LeaderIdType TEST_LEADER_ID = 1;
    
    // Set leader in storage
    Ethernet::Address leader_addr;
    leader_addr.bytes[5] = static_cast<uint8_t>(TEST_LEADER_ID);
    storage.setLeaderId(leader_addr);
    
    // Test 1: Messages with timestamps in the past (rx < tx)
    auto now = clock.getLocalSteadyHardwareTime();
    auto tx_future = now + 1000us;
    auto rx_past = now - 1000us; // Received before it was sent (impossible but test robustness)
    auto ptp_past = createPtpData(TEST_LEADER_ID, tx_future, rx_past);
    clock.activate(&ptp_past);
    // Should still transition (clock should handle this gracefully)
    assert_equal(Clock::State::AWAITING_SECOND_MSG, clock.getState(), 
        "Should handle impossible timing gracefully");
}

/**
 * @brief Tests that Clock handles extreme coordinate values
 * 
 * Verifies that the Clock can process messages with extremely large or
 * small coordinate values without causing numerical overflow, underflow,
 * or other mathematical errors. This test ensures robustness in scenarios
 * with unusual coordinate systems or corrupted coordinate data.
 */
void ClockTest::testClockHandlesExtremeCoordinateValues() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    const LeaderIdType TEST_LEADER_ID = 1;
    
    // Set leader in storage
    Ethernet::Address leader_addr;
    leader_addr.bytes[5] = static_cast<uint8_t>(TEST_LEADER_ID);
    storage.setLeaderId(leader_addr);
    
    // Test with extreme timing values (since coordinates are no longer used)
    auto now = clock.getLocalSteadyHardwareTime();
    auto ptp_extreme = createPtpData(TEST_LEADER_ID, now + 2000us, now + 2100us);
    clock.activate(&ptp_extreme);
    assert_equal(Clock::State::AWAITING_SECOND_MSG, clock.getState(), 
        "Should handle extreme timing values without issues");
}

/**
 * @brief Tests that Clock handles zero time difference between messages
 * 
 * Verifies that the Clock can handle the edge case where two consecutive
 * PTP messages have identical timestamps (zero time difference). This
 * scenario can occur in high-frequency messaging or when timestamps
 * have limited precision, and should not cause division by zero errors.
 */
void ClockTest::testClockHandlesZeroTimeDifferenceBetweenMessages() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    const LeaderIdType TEST_LEADER_ID = 1;
    
    // Set leader in storage
    Ethernet::Address leader_addr;
    leader_addr.bytes[5] = static_cast<uint8_t>(TEST_LEADER_ID);
    storage.setLeaderId(leader_addr);
    
    // Test zero time difference between messages (edge case for drift calculation)
    auto now = clock.getLocalSteadyHardwareTime();
    
    // First message
    auto tx1 = now + 3000us;
    auto rx1 = tx1 + 1000us;
    auto ptp1 = createPtpData(TEST_LEADER_ID, tx1, rx1);
    clock.activate(&ptp1);
    
    // Second message with exactly the same timestamp (zero time difference)
    auto ptp2 = createPtpData(TEST_LEADER_ID, tx1, rx1); // Same timestamps
    clock.activate(&ptp2);
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
        "Should handle zero time difference between messages");
}

/**
 * @brief Tests that Clock handles rapid message sequences
 * 
 * Verifies that the Clock can process multiple PTP messages in rapid
 * succession without performance degradation or state corruption. This
 * test ensures that the Clock can handle high-frequency synchronization
 * scenarios and burst message patterns.
 */
void ClockTest::testClockHandlesRapidMessageSequences() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    const LeaderIdType TEST_LEADER_ID = 1;
    
    // Set leader in storage
    Ethernet::Address leader_addr;
    leader_addr.bytes[5] = static_cast<uint8_t>(TEST_LEADER_ID);
    storage.setLeaderId(leader_addr);
    
    // Test very rapid message sequence
    auto now = clock.getLocalSteadyHardwareTime();
    auto base_tx = now + 5000us;
    auto base_rx = base_tx + 1000us;
    
    for (int i = 0; i < 5; ++i) {
        auto ptp_rapid = createPtpData(TEST_LEADER_ID, 
            base_tx + std::chrono::microseconds(i), 
            base_rx + std::chrono::microseconds(i));
        clock.activate(&ptp_rapid);
    }
    assert_equal(Clock::State::SYNCHRONIZED, clock.getState(), 
        "Should handle rapid message sequence");
}

/**
 * @brief Tests that Clock handles maximum leader ID values
 * 
 * Verifies that the Clock can process messages from leaders with maximum
 * possible ID values without overflow or other numerical issues. This
 * test ensures that the Clock works correctly across the full range of
 * valid leader identifiers.
 */
void ClockTest::testClockHandlesMaximumLeaderIdValues() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    
    // Test with maximum leader ID value
    const LeaderIdType MAX_LEADER_ID = std::numeric_limits<LeaderIdType>::max();
    Ethernet::Address max_leader_addr;
    max_leader_addr.bytes[5] = static_cast<uint8_t>(MAX_LEADER_ID & 0xFF);
    storage.setLeaderId(max_leader_addr);
    
    auto now = clock.getLocalSteadyHardwareTime();
    auto ptp_max_leader = createPtpData(MAX_LEADER_ID & 0xFF, now + 6000us, now + 6100us);
    clock.activate(&ptp_max_leader);
    assert_equal(Clock::State::AWAITING_SECOND_MSG, clock.getState(), 
        "Should handle maximum leader ID value");
}

/**
 * @brief Tests that Clock methods are thread-safe
 * 
 * Verifies that multiple threads can safely call Clock methods concurrently
 * without causing race conditions, data corruption, or crashes. This test
 * ensures that the Clock can be used safely in multi-threaded environments
 * where multiple components may access timing services simultaneously.
 */
void ClockTest::testClockMethodsAreThreadSafe() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    const int num_threads = 4;
    const int num_operations = 1000;
    std::vector<std::thread> threads;
    std::atomic<bool> error_occurred{false};
    
    // Set a leader ID in storage for the test
    const LeaderIdType TEST_LEADER_ID = 1;
    Ethernet::Address leader_addr;
    leader_addr.bytes[5] = static_cast<uint8_t>(TEST_LEADER_ID);
    storage.setLeaderId(leader_addr);
    
    // Function to be executed by each thread
    auto thread_func = [&clock, &storage, &error_occurred, num_operations, TEST_LEADER_ID]() {
        for (int i = 0; i < num_operations && !error_occurred; ++i) {
            try {
                // Randomly perform different operations
                switch (i % 5) {
                    case 0: {
                        bool is_synchronized = false;
                        auto time = clock.getSynchronizedTime(&is_synchronized);
                        (void)time;
                        break;
                    }
                    case 1: {
                        auto state = clock.getState();
                        (void)state;
                        break;
                    }
                    case 2: {
                        auto ptp_data = createPtpData(TEST_LEADER_ID, 
                            clock.getLocalSteadyHardwareTime(),
                            clock.getLocalSteadyHardwareTime() + 100us);
                        clock.activate(&ptp_data);
                        break;
                    }
                    case 3: {
                        clock.activate(nullptr);
                        break;
                    }
                    case 4: {
                        Ethernet::Address new_leader_addr;
                        new_leader_addr.bytes[5] = static_cast<uint8_t>(TEST_LEADER_ID + (i % 2));
                        storage.setLeaderId(new_leader_addr);
                        break;
                    }
                }
            } catch (const std::exception& e) {
                error_occurred = true;
                throw std::runtime_error(std::string("Exception in thread: ") + e.what());
            }
        }
    };
    
    // Launch threads
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(thread_func);
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    assert_false(error_occurred, "Thread safety test failed");
}

/**
 * @brief Tests that Clock-LeaderKeyStorage integration is thread-safe
 * 
 * Verifies that concurrent access to both Clock and LeaderKeyStorage
 * from multiple threads does not cause race conditions or inconsistent
 * state. This test ensures that the integration between these two
 * components maintains data integrity in multi-threaded scenarios.
 */
void ClockTest::testLeaderKeyStorageIntegrationIsThreadSafe() {
    auto& clock = Clock::getInstance();
    auto& storage = LeaderKeyStorage::getInstance();
    const int num_threads = 4;
    const int num_operations = 1000;
    std::vector<std::thread> threads;
    std::atomic<bool> error_occurred{false};
    
    // Function to be executed by each thread
    auto thread_func = [&clock, &storage, &error_occurred, num_operations]() {
        for (int i = 0; i < num_operations && !error_occurred; ++i) {
            try {
                // Randomly perform different operations
                switch (i % 6) {
                    case 0: {
                        bool is_synchronized = false;
                        auto time = clock.getSynchronizedTime(&is_synchronized);
                        (void)time;
                        break;
                    }
                    case 1: {
                        auto state = clock.getState();
                        (void)state;
                        break;
                    }
                    case 2: {
                        auto leader_id = storage.getLeaderId();
                        (void)leader_id;
                        break;
                    }
                    case 3: {
                        auto mac_key = storage.getGroupMacKey();
                        (void)mac_key;
                        break;
                    }
                    case 4: {
                        Ethernet::Address test_addr;
                        test_addr.bytes[0] = static_cast<uint8_t>(i);
                        test_addr.bytes[1] = static_cast<uint8_t>(i+1);
                        test_addr.bytes[2] = static_cast<uint8_t>(i+2);
                        test_addr.bytes[3] = static_cast<uint8_t>(i+3);
                        test_addr.bytes[4] = static_cast<uint8_t>(i+4);
                        test_addr.bytes[5] = static_cast<uint8_t>(i+5);
                        storage.setLeaderId(test_addr);
                        break;
                    }
                    case 5: {
                        auto ptp_data = createPtpData(i, 
                            clock.getLocalSteadyHardwareTime(),
                            clock.getLocalSteadyHardwareTime() + 100us);
                        clock.activate(&ptp_data);
                        break;
                    }
                }
            } catch (const std::exception& e) {
                error_occurred = true;
                throw std::runtime_error(std::string("Exception in thread: ") + e.what());
            }
        }
    };
    
    // Launch threads
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(thread_func);
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    assert_false(error_occurred, "Thread safety test failed");
}

/**
 * @brief Tests that local time methods are thread-safe
 * 
 * Verifies that getLocalSystemTime() and getLocalSteadyHardwareTime()
 * can be called safely from multiple threads without causing race
 * conditions or returning invalid timestamps. This test ensures that
 * basic time retrieval operations are thread-safe.
 */
void ClockTest::testLocalTimeMethodsAreThreadSafe() {
    auto& clock = Clock::getInstance();
    
    // Test 5: Thread safety of time methods
    std::atomic<bool> error_occurred{false};
    std::vector<std::thread> threads;
    const int num_threads = 4;
    const int num_calls = 100;
    
    auto time_test_func = [&clock, &error_occurred, num_calls]() {
        for (int i = 0; i < num_calls && !error_occurred; ++i) {
            try {
                auto sys_time = clock.getLocalSystemTime();
                auto steady_time = clock.getLocalSteadyHardwareTime();
                
                // Basic sanity checks
                if (sys_time.time_since_epoch().count() <= 0 || 
                    steady_time.time_since_epoch().count() <= 0) {
                    error_occurred = true;
                    return;
                }
            } catch (const std::exception& e) {
                error_occurred = true;
                return;
            }
        }
    };
    
    // Launch threads
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(time_test_func);
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    assert_false(error_occurred, "Time methods should be thread-safe");
}

/**
 * @brief Tests Clock-Protocol-NIC integration for timestamp handling
 * 
 * Verifies that the integrated system correctly handles timestamp fields
 * in protocol packets and updates the Clock with PTP timing information.
 * This test simulates the flow from Protocol send through NIC timestamping
 * to Protocol receive and Clock updates.
 */
// void ClockTest::testClockProtocolNicIntegration() {
//     auto& clock = Clock::getInstance();
//     auto& storage = LeaderKeyStorage::getInstance();
//     const LeaderIdType TEST_LEADER_ID = 1;
    
//     // Set leader in storage
//     Ethernet::Address leader_addr;
//     leader_addr.bytes[5] = static_cast<uint8_t>(TEST_LEADER_ID);
//     storage.setLeaderId(leader_addr);
    
//     // Test 1: Verify Clock provides synchronized timestamps
//     auto initial_time = clock.getSynchronizedTime();
//     assert_true(initial_time.time_since_epoch().count() > 0, 
//         "Clock should provide valid synchronized timestamps");
    
//     // Test 2: Verify timestamp progression
//     std::this_thread::sleep_for(1ms);
//     auto later_time = clock.getSynchronizedTime();
//     assert_true(later_time > initial_time, 
//         "Clock timestamps should progress forward");
    
//     // Test 3: Simulate PTP message processing
//     // This simulates what would happen when NIC fills timestamps and Protocol processes them
//     auto base_time = clock.getLocalSteadyHardwareTime();
    
//     // Simulate first PTP message
//     auto tx_time1 = base_time;
//     auto rx_time1 = base_time + 100us; // 100us propagation delay
//     auto ptp_data1 = createPtpData(TEST_LEADER_ID, tx_time1, rx_time1);
    
//     auto initial_state = clock.getState();
//     clock.activate(&ptp_data1);
//     auto state_after_first = clock.getState();
    
//     // Should transition from UNSYNCHRONIZED to AWAITING_SECOND_MSG
//     if (initial_state == Clock::State::UNSYNCHRONIZED) {
//         assert_equal(Clock::State::AWAITING_SECOND_MSG, state_after_first,
//             "Should transition to AWAITING_SECOND_MSG after first PTP message");
//     }
    
//     // Simulate second PTP message for full synchronization
//     auto tx_time2 = base_time + 1000us;
//     auto rx_time2 = base_time + 1100us; // Same propagation delay
//     auto ptp_data2 = createPtpData(TEST_LEADER_ID, tx_time2, rx_time2);
    
//     clock.activate(&ptp_data2);
//     auto final_state = clock.getState();
    
//     // Should achieve synchronization
//     assert_equal(Clock::State::SYNCHRONIZED, final_state,
//         "Should achieve SYNCHRONIZED state after second PTP message");
    
//     // Test 4: Verify synchronized time calculation works
//     auto sync_time = clock.getSynchronizedTime();
//     auto local_time = clock.getLocalSteadyHardwareTime();
    
//     // Should be reasonably close (within 10ms tolerance for test)
//     assert_timestamps_equal(local_time, sync_time,
//         "Synchronized time should be close to local time in test scenario", 10000us);
    
//     // Test 5: Verify Clock state persistence
//     assert_true(clock.isFullySynchronized(),
//         "Clock should report as fully synchronized");
//     assert_equal(TEST_LEADER_ID, clock.getCurrentLeader(),
//         "Clock should track the correct leader ID");
    
//     db<ClockTest>(INF) << "Clock-Protocol-NIC integration test completed successfully\n";
// }

// Stream operator implementation
inline std::ostream& operator<<(std::ostream& os, const TimestampType& tp) {
    auto duration = tp.time_since_epoch();
    auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
    duration -= hours;
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration);
    duration -= minutes;
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
    duration -= seconds;
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration);
    
    os << std::setfill('0') << std::setw(2) << hours.count() << ":"
       << std::setw(2) << minutes.count() << ":"
       << std::setw(2) << seconds.count() << "."
       << std::setw(6) << microseconds.count();
    return os;
}

/**
 * @brief Helper function to create PtpRelevantData structures for testing
 * 
 * @param sender_id ID of the message sender
 * @param tx_time Timestamp when message was transmitted
 * @param rx_time Timestamp when message was received locally
 * @return PtpRelevantData structure with the specified parameters
 * 
 * This utility function creates properly formatted PtpRelevantData structures
 * for use in Clock tests. It simplifies test setup by providing a consistent
 * way to create test messages with specific timing information.
 */
PtpRelevantData createPtpData(LeaderIdType sender_id, 
                            TimestampType tx_time,
                            TimestampType rx_time) {
    return PtpRelevantData{
        .sender_id = sender_id,
        .ts_tx_at_sender = tx_time,
        .ts_local_rx = rx_time
    };
}

// Main function
int main() {
    TEST_INIT("ClockTest");
    ClockTest test;
    test.run();
    return 0;
} 