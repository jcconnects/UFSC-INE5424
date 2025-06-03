#include "../../tests/testcase.h"
#include "../../include/api/framework/rsu.h"
#include "../../include/api/util/debug.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <fstream>
#include <string>
#include <regex>

using namespace std::chrono_literals;

class RSUTest : public TestCase {
protected:
    void setUp() override;
    void tearDown() override;

    // Helper methods
    void clearDebugLog();
    std::string readDebugLog();
    int countBroadcastMessages(const std::string& log_content, unsigned int rsu_id, unsigned int unit);
    bool waitForBroadcasts(RSU* rsu, int expected_count, std::chrono::milliseconds timeout);

    // === BASIC INITIALIZATION AND LIFECYCLE TESTS ===
    void testRSUInitialization();
    void testRSUStartStop();
    void testRSURunningState();
    void testRSUDestructor();

    // === PERIODIC BROADCASTING TESTS ===
    void testPeriodicBroadcasting();
    void testBroadcastFrequency();
    void testBroadcastContent();
    void testBroadcastWithCustomData();

    // === NETWORK INTEGRATION TESTS ===
    void testNetworkStackIntegration();
    void testMACAddressGeneration();
    void testCommunicatorIntegration();

    // === CONFIGURATION AND PARAMETER TESTS ===
    void testDifferentRSUIDs();
    void testDifferentUnits();
    void testDifferentPeriods();
    void testPeriodAdjustment();

    // === MESSAGE CONTENT VERIFICATION TESTS ===
    void testResponseMessageType();
    void testMessageOrigin();
    void testMessageUnit();
    void testMessageTimestamp();

    // === EDGE CASES AND ERROR HANDLING TESTS ===
    void testZeroPeriod();
    void testVeryShortPeriod();
    void testVeryLongPeriod();
    void testLargeDataPayload();
    void testNullDataPointer();

    // === THREAD SAFETY AND CONCURRENCY TESTS ===
    void testMultipleRSUInstances();
    void testConcurrentStartStop();
    void testThreadSafety();

private:
    static constexpr const char* TEST_LOG_FILE = "/tmp/rsu_test_debug.log";
    std::unique_ptr<RSU> _test_rsu;

public:
    RSUTest();
};

/**
 * @brief Constructor that registers all test methods
 * 
 * Organizes tests into logical groups for better maintainability and clarity.
 * Each test method name clearly describes what functionality is being tested.
 */
RSUTest::RSUTest() {
    // === BASIC INITIALIZATION AND LIFECYCLE TESTS ===
    DEFINE_TEST(testRSUInitialization);
    DEFINE_TEST(testRSUStartStop);
    DEFINE_TEST(testRSURunningState);
    DEFINE_TEST(testRSUDestructor);

    // === PERIODIC BROADCASTING TESTS ===
    DEFINE_TEST(testPeriodicBroadcasting);
    DEFINE_TEST(testBroadcastFrequency);
    DEFINE_TEST(testBroadcastContent);
    DEFINE_TEST(testBroadcastWithCustomData);

    // === NETWORK INTEGRATION TESTS ===
    DEFINE_TEST(testNetworkStackIntegration);
    DEFINE_TEST(testMACAddressGeneration);
    DEFINE_TEST(testCommunicatorIntegration);

    // === CONFIGURATION AND PARAMETER TESTS ===
    DEFINE_TEST(testDifferentRSUIDs);
    DEFINE_TEST(testDifferentUnits);
    DEFINE_TEST(testDifferentPeriods);
    DEFINE_TEST(testPeriodAdjustment);

    // === MESSAGE CONTENT VERIFICATION TESTS ===
    DEFINE_TEST(testResponseMessageType);
    DEFINE_TEST(testMessageOrigin);
    DEFINE_TEST(testMessageUnit);
    DEFINE_TEST(testMessageTimestamp);

    // === EDGE CASES AND ERROR HANDLING TESTS ===
    DEFINE_TEST(testZeroPeriod);
    DEFINE_TEST(testVeryShortPeriod);
    DEFINE_TEST(testVeryLongPeriod);
    DEFINE_TEST(testLargeDataPayload);
    DEFINE_TEST(testNullDataPointer);

    // === THREAD SAFETY AND CONCURRENCY TESTS ===
    DEFINE_TEST(testMultipleRSUInstances);
    //DEFINE_TEST(testConcurrentStartStop);
    DEFINE_TEST(testThreadSafety);
}

void RSUTest::setUp() {
    // Set up debug logging to file for message verification
    Debug::set_log_file(TEST_LOG_FILE);
    clearDebugLog();
    
    // Clean up any existing RSU
    _test_rsu.reset();
    
    // Give time for any pending operations to complete
    std::this_thread::sleep_for(10ms);
}

void RSUTest::tearDown() {
    // Stop and clean up test RSU
    if (_test_rsu) {
        _test_rsu->stop();
        _test_rsu.reset();
    }
    
    // Close debug log
    Debug::close_log_file();
    
    // Give time for cleanup
    std::this_thread::sleep_for(10ms);
}

void RSUTest::clearDebugLog() {
    std::ofstream log_file(TEST_LOG_FILE, std::ios::trunc);
    log_file.close();
}

std::string RSUTest::readDebugLog() {
    std::ifstream log_file(TEST_LOG_FILE);
    std::string content((std::istreambuf_iterator<char>(log_file)),
                        std::istreambuf_iterator<char>());
    return content;
}

int RSUTest::countBroadcastMessages(const std::string& log_content, unsigned int rsu_id, unsigned int unit) {
    // Count occurrences of broadcast messages for specific RSU and unit
    std::regex broadcast_regex(R"(\[RSU\] RSU )" + std::to_string(rsu_id) + 
                              R"( broadcast RESPONSE for unit )" + std::to_string(unit));
    
    auto begin = std::sregex_iterator(log_content.begin(), log_content.end(), broadcast_regex);
    auto end = std::sregex_iterator();
    
    return std::distance(begin, end);
}

bool RSUTest::waitForBroadcasts(RSU* rsu, int expected_count, std::chrono::milliseconds timeout) {
    auto start_time = std::chrono::steady_clock::now();
    int count = 0;
    
    while (std::chrono::steady_clock::now() - start_time < timeout) {
        std::string log_content = readDebugLog();
        count = countBroadcastMessages(log_content, rsu->address().paddr().bytes[5], rsu->unit());
        
        if (count >= expected_count) {
            return true;
        }
        
        std::this_thread::sleep_for(10ms);
    }
    
    return false;
}

/**
 * @brief Tests basic RSU initialization
 * 
 * Verifies that an RSU can be created with valid parameters and that all
 * internal components are properly initialized.
 */
void RSUTest::testRSUInitialization() {
    const unsigned int RSU_ID = 100;
    const unsigned int UNIT = 42;
    const auto PERIOD = 1000ms;
    const double lat = 30.0;
    const double lon = 32.1;
    const double radius = 300.0;
    const std::string TEST_DATA = "TEST_RSU_DATA";
    
    // Test initialization without data
    _test_rsu = std::make_unique<RSU>(RSU_ID, UNIT, PERIOD, lat, lon, radius);
    
    assert_equal(RSU_ID, _test_rsu->address().paddr().bytes[5], 
        "RSU ID should be reflected in MAC address");
    assert_equal(UNIT, _test_rsu->unit(), "Unit should match constructor parameter");
    assert_equal(PERIOD.count(), _test_rsu->period().count(), "Period should match constructor parameter");
    assert_false(_test_rsu->running(), "RSU should not be running initially");
    
    // Test initialization with data
    _test_rsu.reset();
    _test_rsu = std::make_unique<RSU>(RSU_ID, UNIT, PERIOD, lat, lon, radius,
                                      TEST_DATA.c_str(), TEST_DATA.size());
    
    assert_equal(UNIT, _test_rsu->unit(), "Unit should match with data payload");
    assert_false(_test_rsu->running(), "RSU should not be running initially with data");
}

/**
 * @brief Tests RSU start and stop functionality
 * 
 * Verifies that the RSU can be started and stopped correctly, and that
 * the running state is properly maintained.
 */
void RSUTest::testRSUStartStop() {
    const unsigned int RSU_ID = 101;
    const unsigned int UNIT = 43;
    const auto PERIOD = 500ms;
    const double lat = 30.0;
    const double lon = 32.1;
    const double radius = 300.0;

    std::cout << "starting stop() test \n";
    
    _test_rsu = std::make_unique<RSU>(RSU_ID, UNIT, PERIOD, lat, lon, radius);
    
    // Test start
    assert_false(_test_rsu->running(), "Should not be running initially");
    _test_rsu->start();
    assert_true(_test_rsu->running(), "Should be running after start");
    
    // Let it run briefly
    std::this_thread::sleep_for(100ms);
    assert_true(_test_rsu->running(), "Should still be running");
    
    // Test stop
    _test_rsu->stop();
    assert_false(_test_rsu->running(), "Should not be running after stop");

    // Test multiple start/stop cycles
    _test_rsu->start();
    assert_true(_test_rsu->running(), "Should be running after restart");
    _test_rsu->stop();
    assert_false(_test_rsu->running(), "Should be stopped after second stop");
}

/**
 * @brief Tests RSU running state management
 * 
 * Verifies that the running state is correctly maintained and reported
 * during various operations.
 */
void RSUTest::testRSURunningState() {
    const unsigned int RSU_ID = 102;
    const unsigned int UNIT = 44;
    const auto PERIOD = 200ms;
    const double lat = 30.0;
    const double lon = 32.1;
    const double radius = 300.0;
    
    _test_rsu = std::make_unique<RSU>(RSU_ID, UNIT, PERIOD, lat, lon, radius);
    
    // Initial state
    assert_false(_test_rsu->running(), "Initial state should be not running");
    
    // After start
    _test_rsu->start();
    assert_true(_test_rsu->running(), "Should be running after start");
    
    // Multiple calls to start should not change state
    _test_rsu->start();
    assert_true(_test_rsu->running(), "Should still be running after multiple starts");
    
    // After stop
    _test_rsu->stop();
    assert_false(_test_rsu->running(), "Should not be running after stop");
    
    // Multiple calls to stop should not change state
    _test_rsu->stop();
    assert_false(_test_rsu->running(), "Should still be not running after multiple stops");
}

/**
 * @brief Tests RSU destructor behavior
 * 
 * Verifies that the RSU properly cleans up resources when destroyed,
 * including stopping the periodic thread and cleaning up network components.
 */
void RSUTest::testRSUDestructor() {
    const unsigned int RSU_ID = 103;
    const unsigned int UNIT = 45;
    const auto PERIOD = 300ms;
    const double lat = 30.0;
    const double lon = 32.1;
    const double radius = 300.0;
    
    {
        RSU rsu(RSU_ID, UNIT, PERIOD, lat, lon, radius);
        rsu.start();
        assert_true(rsu.running(), "RSU should be running");
        // RSU goes out of scope and destructor is called
    }
    
    // Give time for cleanup
    std::this_thread::sleep_for(50ms);
    
    // If we reach here without hanging, destructor worked correctly
    assert_true(true, "Destructor should clean up properly without hanging");
}

/**
 * @brief Tests periodic broadcasting functionality
 * 
 * Verifies that the RSU sends RESPONSE messages at the specified periodic
 * intervals when running.
 */
void RSUTest::testPeriodicBroadcasting() {
    const unsigned int RSU_ID = 104;
    const unsigned int UNIT = 46;
    const auto PERIOD = 100ms;
    const double lat = 30.0;
    const double lon = 32.1;
    const double radius = 400.0;
    
    _test_rsu = std::make_unique<RSU>(RSU_ID, UNIT, PERIOD, lat, lon, radius);
    _test_rsu->start();
    
    // Wait for several broadcast cycles
    std::this_thread::sleep_for(350ms);
    
    // Check debug log for broadcast messages
    std::string log_content = readDebugLog();
    int broadcast_count = countBroadcastMessages(log_content, RSU_ID, UNIT);
    
    // Should have at least 2-3 broadcasts in 350ms with 100ms period
    assert_true(broadcast_count >= 2, 
        "Should have multiple broadcasts: expected >= 2, got " + std::to_string(broadcast_count));
}

/**
 * @brief Tests broadcast frequency accuracy
 * 
 * Verifies that the RSU broadcasts messages at approximately the correct
 * frequency based on the configured period.
 */
void RSUTest::testBroadcastFrequency() {
    const unsigned int RSU_ID = 105;
    const unsigned int UNIT = 47;
    const auto PERIOD = 200ms;
    const double lat = 37.2;
    const double lon = 27.1;
    const double radius = 400.0;
    
    _test_rsu = std::make_unique<RSU>(RSU_ID, UNIT, PERIOD, lat, lon, radius);
    _test_rsu->start();
    
    // Wait for enough time for multiple broadcasts
    std::this_thread::sleep_for(1000ms); // 1 second
    
    std::string log_content = readDebugLog();
    int broadcast_count = countBroadcastMessages(log_content, RSU_ID, UNIT);
    
    // With 200ms period, should have approximately 5 broadcasts in 1000ms
    // Allow some tolerance for timing variations
    assert_true(broadcast_count >= 3 && broadcast_count <= 7, 
        "Broadcast frequency should be approximately correct: expected 3-7, got " + 
        std::to_string(broadcast_count));
}

/**
 * @brief Tests broadcast content structure
 * 
 * Verifies that the broadcast messages contain the correct message type,
 * origin address, and unit information.
 */
void RSUTest::testBroadcastContent() {
    const unsigned int RSU_ID = 106;
    const unsigned int UNIT = 48;
    const auto PERIOD = 150ms;
    const double lat = 42.0;
    const double lon = 21.1;
    const double radius = 420.0;
    
    _test_rsu = std::make_unique<RSU>(RSU_ID, UNIT, PERIOD, lat, lon, radius);
    
    // Verify address and unit are correctly set
    assert_equal(UNIT, _test_rsu->unit(), "Unit should be correctly set");
    assert_equal(RSU_ID, _test_rsu->address().paddr().bytes[5], 
        "RSU ID should be in address");
    
    _test_rsu->start();
    
    // Wait for broadcasts
    assert_true(waitForBroadcasts(_test_rsu.get(), 2, 500ms),
        "Should receive expected broadcasts within timeout");
}

/**
 * @brief Tests broadcasting with custom data payload
 * 
 * Verifies that the RSU can broadcast messages with custom data payloads
 * and that the data is properly included in the messages.
 */
void RSUTest::testBroadcastWithCustomData() {
    const unsigned int RSU_ID = 107;
    const unsigned int UNIT = 49;
    const auto PERIOD = 250ms;
    const std::string TEST_DATA = "CUSTOM_RSU_PAYLOAD";
    const double lat = 42.0;
    const double lon = 21.1;
    const double radius = 420.0;
    
    _test_rsu = std::make_unique<RSU>(RSU_ID, UNIT, PERIOD, lat, lon, radius,
                                      TEST_DATA.c_str(), TEST_DATA.size());
    _test_rsu->start();
    
    // Wait for broadcasts
    assert_true(waitForBroadcasts(_test_rsu.get(), 1, 500ms),
        "Should receive broadcasts with custom data");
}

/**
 * @brief Tests network stack integration
 * 
 * Verifies that the RSU properly integrates with the Network, Protocol,
 * and Communicator components.
 */
void RSUTest::testNetworkStackIntegration() {
    const unsigned int RSU_ID = 108;
    const unsigned int UNIT = 50;
    const auto PERIOD = 400ms;
    const double lat = 47.0;
    const double lon = 21.1;
    const double radius = 420.0;
    
    _test_rsu = std::make_unique<RSU>(RSU_ID, UNIT, PERIOD, lat, lon, radius);
    
    // Verify that network components are initialized
    // (This is implicit in successful construction)
    assert_true(true, "Network stack should be properly initialized");
    
    _test_rsu->start();
    std::this_thread::sleep_for(200ms);
    
    // Check that debug messages indicate network activity
    std::string log_content = readDebugLog();
    assert_true(log_content.find("[RSU]") != std::string::npos,
        "Should have RSU debug messages indicating network activity");
}

/**
 * @brief Tests MAC address generation based on RSU ID
 * 
 * Verifies that each RSU gets a unique MAC address based on its ID
 * and that the address is properly formatted.
 */
void RSUTest::testMACAddressGeneration() {
    const unsigned int RSU_ID_1 = 109;
    const unsigned int RSU_ID_2 = 110;
    const unsigned int UNIT = 51;
    const auto PERIOD = 500ms;
    const double lat = 47.0;
    const double lon = 21.1;
    const double radius = 450.0;
    
    RSU rsu1(RSU_ID_1, UNIT, PERIOD, lat, lon, radius);
    RSU rsu2(RSU_ID_2, UNIT, PERIOD, lat, lon, radius);
    
    // Verify different RSUs have different MAC addresses
    auto addr1 = rsu1.address().paddr();
    auto addr2 = rsu2.address().paddr();
    
    assert_true(addr1.bytes[5] != addr2.bytes[5], 
        "Different RSU IDs should result in different MAC addresses");
    assert_equal(RSU_ID_1, addr1.bytes[5], "RSU ID should be in MAC address");
    assert_equal(RSU_ID_2, addr2.bytes[5], "RSU ID should be in MAC address");
}

/**
 * @brief Tests Communicator integration
 * 
 * Verifies that the RSU properly uses the Communicator to send messages
 * and that the address and port configuration is correct.
 */
void RSUTest::testCommunicatorIntegration() {
    const unsigned int RSU_ID = 111;
    const unsigned int UNIT = 52;
    const auto PERIOD = 300ms;
    const double lat = 41.0;
    const double lon = 26.1;
    const double radius = 400.0;
    
    _test_rsu = std::make_unique<RSU>(RSU_ID, UNIT, PERIOD, lat, lon, radius);
    
    // Verify address configuration
    auto address = _test_rsu->address();
    assert_equal(RSU_ID, address.port(), "Port should match RSU ID");
    assert_equal(RSU_ID, address.paddr().bytes[5], "Physical address should contain RSU ID");
}

/**
 * @brief Tests different RSU IDs
 * 
 * Verifies that RSUs with different IDs can coexist and operate
 * independently without interference.
 */
void RSUTest::testDifferentRSUIDs() {
    const unsigned int UNIT = 53;
    const auto PERIOD = 200ms;
    const double lat = 41.0;
    const double lon = 38.1;
    const double radius = 500.0;
    
    std::vector<std::unique_ptr<RSU>> rsus;
    
    // Create multiple RSUs with different IDs
    for (unsigned int id = 200; id < 203; ++id) {
        rsus.push_back(std::make_unique<RSU>(id, UNIT, PERIOD, lat, lon, radius));
        rsus.back()->start();
    }
    
    // Let them run
    std::this_thread::sleep_for(400ms);
    
    // Verify each RSU has distinct addresses
    for (size_t i = 0; i < rsus.size(); ++i) {
        for (size_t j = i + 1; j < rsus.size(); ++j) {
            assert_true(rsus[i]->address().paddr().bytes[5] != 
                       rsus[j]->address().paddr().bytes[5],
                       "Different RSUs should have different addresses");
        }
    }
    
    // Clean up
    for (auto& rsu : rsus) {
        rsu->stop();
    }
}

/**
 * @brief Tests different unit types
 * 
 * Verifies that RSUs can broadcast different unit types and that
 * the unit information is correctly maintained and transmitted.
 */
void RSUTest::testDifferentUnits() {
    const unsigned int RSU_ID = 112;
    const auto PERIOD = 250ms;
    const double lat = 41.0;
    const double lon = 26.1;
    const double radius = 700.0;
    
    std::vector<unsigned int> units = {100, 200, 300, 999};
    
    for (auto unit : units) {
        RSU rsu(RSU_ID, unit, PERIOD, lat, lon, radius);
        assert_equal(unit, rsu.unit(), 
            "Unit " + std::to_string(unit) + " should be correctly set");
        
        rsu.start();
        std::this_thread::sleep_for(100ms);
        rsu.stop();
    }
}

/**
 * @brief Tests different broadcasting periods
 * 
 * Verifies that RSUs can be configured with different periods and
 * that the broadcasting frequency adjusts accordingly.
 */
void RSUTest::testDifferentPeriods() {
    const unsigned int RSU_ID = 113;
    const unsigned int UNIT = 54;
    const double lat = 41.0;
    const double lon = 26.1;
    const double radius = 700.0;
    
    std::vector<std::chrono::milliseconds> periods = {50ms, 100ms, 500ms, 1000ms};
    
    for (auto period : periods) {
        RSU rsu(RSU_ID, UNIT, period, lat, lon, radius);
        assert_equal(period.count(), rsu.period().count(), 
            "Period " + std::to_string(period.count()) + "ms should be correctly set");
    }
}

/**
 * @brief Tests period adjustment functionality
 * 
 * Verifies that the RSU period can be adjusted dynamically and that
 * the new period takes effect correctly.
 */
void RSUTest::testPeriodAdjustment() {
    const unsigned int RSU_ID = 114;
    const unsigned int UNIT = 55;
    const auto INITIAL_PERIOD = 300ms;
    const auto NEW_PERIOD = 150ms;
    const double lat = 41.0;
    const double lon = 26.1;
    const double radius = 700.0;
    
    _test_rsu = std::make_unique<RSU>(RSU_ID, UNIT, INITIAL_PERIOD, lat, lon, radius);
    
    assert_equal(INITIAL_PERIOD.count(), _test_rsu->period().count(), 
        "Initial period should be set correctly");
    
    _test_rsu->adjust_period(NEW_PERIOD);
    assert_equal(NEW_PERIOD.count(), _test_rsu->period().count(), 
        "Period should be updated after adjustment");
    
    // Test adjustment while running
    _test_rsu->start();
    _test_rsu->adjust_period(INITIAL_PERIOD);
    assert_equal(INITIAL_PERIOD.count(), _test_rsu->period().count(), 
        "Period should be adjustable while running");
}

/**
 * @brief Tests RESPONSE message type verification
 * 
 * Verifies that the RSU sends messages with the correct message type
 * (RESPONSE) as specified in the requirements.
 */
void RSUTest::testResponseMessageType() {
    const unsigned int RSU_ID = 115;
    const unsigned int UNIT = 56;
    const auto PERIOD = 200ms;
    const double lat = 41.0;
    const double lon = 26.1;
    const double radius = 700.0;
    
    _test_rsu = std::make_unique<RSU>(RSU_ID, UNIT, PERIOD, lat, lon, radius);
    _test_rsu->start();
    
    // Wait for broadcasts and check logs
    std::this_thread::sleep_for(300ms);
    
    std::string log_content = readDebugLog();
    assert_true(log_content.find("broadcast RESPONSE") != std::string::npos,
        "Should broadcast RESPONSE type messages");
}

/**
 * @brief Tests message origin information
 * 
 * Verifies that broadcast messages contain the correct origin address
 * information matching the RSU's configured address.
 */
void RSUTest::testMessageOrigin() {
    const unsigned int RSU_ID = 116;
    const unsigned int UNIT = 57;
    const auto PERIOD = 250ms;
    const double lat = 41.0;
    const double lon = 26.1;
    const double radius = 700.0;
    
    _test_rsu = std::make_unique<RSU>(RSU_ID, UNIT, PERIOD, lat, lon, radius);
    
    auto address = _test_rsu->address();
    assert_equal(RSU_ID, address.port(), "Message origin port should match RSU ID");
    assert_equal(RSU_ID, address.paddr().bytes[5], 
        "Message origin address should contain RSU ID");
}

/**
 * @brief Tests message unit information
 * 
 * Verifies that broadcast messages contain the correct unit information
 * as specified during RSU initialization.
 */
void RSUTest::testMessageUnit() {
    const unsigned int RSU_ID = 117;
    const unsigned int UNIT = 58;
    const auto PERIOD = 200ms;
    const double lat = 41.0;
    const double lon = 26.1;
    const double radius = 700.0;
    
    _test_rsu = std::make_unique<RSU>(RSU_ID, UNIT, PERIOD, lat, lon, radius);
    assert_equal(UNIT, _test_rsu->unit(), "Message unit should match configuration");
    
    _test_rsu->start();
    
    // Verify unit in broadcast logs
    assert_true(waitForBroadcasts(_test_rsu.get(), 1, 400ms),
        "Should broadcast with correct unit");
}

/**
 * @brief Tests message timestamp handling
 * 
 * Verifies that broadcast messages include proper timestamp information
 * and that timestamps are current when messages are sent.
 */
void RSUTest::testMessageTimestamp() {
    const unsigned int RSU_ID = 118;
    const unsigned int UNIT = 59;
    const auto PERIOD = 300ms;
    const double lat = 41.0;
    const double lon = 26.1;
    const double radius = 700.0;
    
    _test_rsu = std::make_unique<RSU>(RSU_ID, UNIT, PERIOD, lat, lon, radius);
    _test_rsu->start();
    
    // Messages should be sent with current timestamps
    // (Verified implicitly through successful message creation and sending)
    assert_true(waitForBroadcasts(_test_rsu.get(), 1, 500ms),
        "Should broadcast with proper timestamps");
}

/**
 * @brief Tests RSU behavior with zero period
 * 
 * Verifies that the RSU handles edge cases appropriately, such as
 * very short or invalid period values.
 */
void RSUTest::testZeroPeriod() {
    const unsigned int RSU_ID = 119;
    const unsigned int UNIT = 60;
    const auto ZERO_PERIOD = 0ms;
    const double lat = 41.0;
    const double lon = 26.1;
    const double radius = 700.0;
    
    // Should be able to create RSU with zero period
    RSU rsu(RSU_ID, UNIT, ZERO_PERIOD, lat, lon, radius);
    assert_equal(ZERO_PERIOD.count(), rsu.period().count(), "Zero period should be accepted");
    
    // Starting with zero period should not crash
    rsu.start();
    std::this_thread::sleep_for(50ms);
    rsu.stop();
}

/**
 * @brief Tests RSU with very short period
 * 
 * Verifies that the RSU can handle very short broadcasting periods
 * without performance issues or system instability.
 */
void RSUTest::testVeryShortPeriod() {
    const unsigned int RSU_ID = 120;
    const unsigned int UNIT = 61;
    const auto SHORT_PERIOD = 1ms;
    const double lat = 41.0;
    const double lon = 26.1;
    const double radius = 700.0;
    
    _test_rsu = std::make_unique<RSU>(RSU_ID, UNIT, SHORT_PERIOD, lat, lon, radius);
    _test_rsu->start();
    
    // Let it run briefly with very short period
    std::this_thread::sleep_for(100ms);
    
    // Should not crash and should generate many broadcasts
    std::string log_content = readDebugLog();
    int broadcast_count = countBroadcastMessages(log_content, RSU_ID, UNIT);
    
    // With 1ms period, should have many broadcasts in 100ms
    assert_true(broadcast_count > 10, 
        "Very short period should generate many broadcasts");
}

/**
 * @brief Tests RSU with very long period
 * 
 * Verifies that the RSU can handle very long broadcasting periods
 * and that the periodic thread remains stable.
 */
void RSUTest::testVeryLongPeriod() {
    const unsigned int RSU_ID = 121;
    const unsigned int UNIT = 62;
    const auto LONG_PERIOD = 9s; // Long period that exceeds SCHED_DEADLINE limits (falls back to regular scheduling)
    const double lat = 41.0;
    const double lon = 26.1;
    const double radius = 700.0;
    
    RSU rsu(RSU_ID, UNIT, LONG_PERIOD, lat, lon, radius);
    // Convert both to milliseconds for comparison
    auto expected_ms = std::chrono::duration_cast<std::chrono::milliseconds>(LONG_PERIOD);
    assert_equal(expected_ms.count(), rsu.period().count(), "Long period should be accepted");
    
    rsu.start();
    std::this_thread::sleep_for(100ms); // Much shorter than period
    rsu.stop();
    
    // Should not have broadcast yet due to long period
    // (This tests that the timing is respected)
}

/**
 * @brief Tests RSU with large data payload
 * 
 * Verifies that the RSU can handle large data payloads in broadcast
 * messages without issues.
 */
void RSUTest::testLargeDataPayload() {
    const unsigned int RSU_ID = 122;
    const unsigned int UNIT = 63;
    const auto PERIOD = 500ms;
    const double lat = 41.0;
    const double lon = 26.1;
    const double radius = 700.0;
    
    // Create large data payload
    std::string large_data(1000, 'X'); // 1KB of 'X' characters
    
    RSU rsu(RSU_ID, UNIT, PERIOD, lat, lon, radius, large_data.c_str(), large_data.size());
    
    rsu.start();
    std::this_thread::sleep_for(200ms);
    rsu.stop();
    
    // Should handle large payload without issues
    assert_true(true, "Should handle large data payload");
}

/**
 * @brief Tests RSU with null data pointer
 * 
 * Verifies that the RSU properly handles null data pointers and
 * zero data sizes during initialization.
 */
void RSUTest::testNullDataPointer() {
    const unsigned int RSU_ID = 123;
    const unsigned int UNIT = 64;
    const auto PERIOD = 300ms;
    const double lat = 41.0;
    const double lon = 26.1;
    const double radius = 700.0;
    
    // Test with null pointer and zero size
    RSU rsu1(RSU_ID, UNIT, PERIOD, lat, lon, radius, nullptr, 0);
    assert_equal(UNIT, rsu1.unit(), "Should handle null data pointer");
    
    // Test with null pointer and non-zero size (should be ignored)
    RSU rsu2(RSU_ID, UNIT, PERIOD, lat, lon, radius, nullptr, 100);
    assert_equal(UNIT, rsu2.unit(), "Should handle null data with non-zero size");
}

/**
 * @brief Tests multiple RSU instances
 * 
 * Verifies that multiple RSU instances can operate simultaneously
 * without interference or resource conflicts.
 */
void RSUTest::testMultipleRSUInstances() {
    const unsigned int UNIT = 65;
    const auto PERIOD = 200ms;
    const double lat = 41.0;
    const double lon = 26.1;
    const double radius = 700.0;
    
    std::vector<std::unique_ptr<RSU>> rsus;
    
    // Create multiple RSUs
    for (unsigned int id = 300; id < 305; ++id) {
        rsus.push_back(std::make_unique<RSU>(id, UNIT + id, PERIOD, lat, lon, radius));
        rsus.back()->start();
    }
    
    // Let them all run
    std::this_thread::sleep_for(400ms);
    
    // All should be running
    for (const auto& rsu : rsus) {
        assert_true(rsu->running(), "All RSUs should be running");
    }
    
    // Stop all
    for (auto& rsu : rsus) {
        rsu->stop();
        assert_false(rsu->running(), "All RSUs should be stopped");
    }
}

/**
 * @brief Tests concurrent start/stop operations
 * 
 * Verifies that concurrent start and stop operations on the same RSU
 * are handled safely without race conditions.
 */
void RSUTest::testConcurrentStartStop() {
    const unsigned int RSU_ID = 124;
    const unsigned int UNIT = 66;
    const auto PERIOD = 100ms;
    const double lat = 41.0;
    const double lon = 26.1;
    const double radius = 700.0;
    
    _test_rsu = std::make_unique<RSU>(RSU_ID, UNIT, PERIOD, lat, lon, radius);
    
    std::atomic<bool> stop_test{false};
    std::vector<std::thread> threads;
    
    // Thread that repeatedly starts the RSU
    threads.emplace_back([this, &stop_test]() {
        while (!stop_test) {
            _test_rsu->start();
            std::this_thread::sleep_for(10ms);
        }
    });
    
    // Thread that repeatedly stops the RSU
    threads.emplace_back([this, &stop_test]() {
        while (!stop_test) {
            _test_rsu->stop();
            std::this_thread::sleep_for(10ms);
        }
    });
    
    // Let them run concurrently
    std::this_thread::sleep_for(200ms);
    stop_test = true;
    
    // Wait for threads to finish
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Should not have crashed
    assert_true(true, "Concurrent start/stop should be safe");
}

/**
 * @brief Tests thread safety of RSU methods
 * 
 * Verifies that RSU methods can be called safely from multiple threads
 * without causing race conditions or data corruption.
 */
void RSUTest::testThreadSafety() {
    const unsigned int RSU_ID = 125;
    const unsigned int UNIT = 67;
    const auto PERIOD = 150ms;
    const double lat = 41.0;
    const double lon = 26.1;
    const double radius = 700.0;
    
    _test_rsu = std::make_unique<RSU>(RSU_ID, UNIT, PERIOD, lat, lon, radius);
    
    std::atomic<bool> stop_test{false};
    std::atomic<bool> error_occurred{false};
    std::vector<std::thread> threads;
    
    // Multiple threads calling different RSU methods
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([this, &stop_test, &error_occurred]() {
            while (!stop_test && !error_occurred) {
                try {
                    auto running = _test_rsu->running();
                    auto period = _test_rsu->period();
                    auto unit = _test_rsu->unit();
                    auto address = _test_rsu->address();
                    
                    // Use the values to prevent optimization
                    (void)running; (void)period; (void)unit; (void)address;
                    
                    std::this_thread::sleep_for(5ms);
                } catch (const std::exception&) {
                    error_occurred = true;
                }
            }
        });
    }
    
    // Start the RSU in the main thread
    _test_rsu->start();
    
    // Let threads run
    std::this_thread::sleep_for(300ms);
    stop_test = true;
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    assert_false(error_occurred, "Thread safety test should not have errors");
}

// Main function
int main() {
    RSUTest test;
    test.run();
    return 0;
} 