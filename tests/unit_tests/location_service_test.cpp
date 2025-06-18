#include <thread>
#include <chrono>
#include <fstream>
#include <filesystem>

#include "../testcase.h"
#include "api/framework/location_service.h"

class LocationServiceTest : public TestCase {
    public:
        LocationServiceTest();
        ~LocationServiceTest() = default;

        void setUp() override;
        void tearDown() override;

        /* TESTS */
        void test_load_trajectory();
        void test_get_specific_coordinates();
        void test_get_coordinates_at_runtime();
        void test_get_trajectory_duration();

    private:
        std::string temp_trajectory_file;
        void createTestTrajectoryFile();
        void cleanupTestTrajectoryFile();
};

LocationServiceTest::LocationServiceTest() {
    DEFINE_TEST(test_load_trajectory);
    DEFINE_TEST(test_get_specific_coordinates);
    DEFINE_TEST(test_get_coordinates_at_runtime);
    DEFINE_TEST(test_get_trajectory_duration);
}

void LocationServiceTest::setUp() {
    // Create temporary directory if it doesn't exist
    std::filesystem::create_directories("tests/temp");
    temp_trajectory_file = "tests/temp/test_trajectory.csv";
    createTestTrajectoryFile();
}

void LocationServiceTest::tearDown() {
    cleanupTestTrajectoryFile();
}

void LocationServiceTest::createTestTrajectoryFile() {
    std::ofstream file(temp_trajectory_file);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to create test trajectory file");
    }
    
    // Write CSV header
    file << "timestamp_ms,x,y\n";
    
    // Create a 60-second trajectory with points every 100ms
    // Vehicle moves from (0,0) to (100,100) over 60 seconds
    for (int i = 0; i <= 600; i++) {
        long long timestamp = i * 100; // Every 100ms
        double progress = static_cast<double>(i) / 600.0; // 0 to 1
        double x = progress * 100.0; // 0 to 100
        double y = progress * 100.0; // 0 to 100
        
        file << timestamp << "," << x << "," << y << "\n";
    }
    
    file.close();
}

void LocationServiceTest::cleanupTestTrajectoryFile() {
    if (std::filesystem::exists(temp_trajectory_file)) {
        std::filesystem::remove(temp_trajectory_file);
    }
    // Also remove temp directory if it's empty
    if (std::filesystem::exists("tests/temp") && std::filesystem::is_empty("tests/temp")) {
        std::filesystem::remove("tests/temp");
    }
}

void LocationServiceTest::test_load_trajectory() {
    // Exercise SUT
    assert_true(LocationService::loadTrajectory(temp_trajectory_file), "LocationService failed to load trajectory file");
}

void LocationServiceTest::test_get_specific_coordinates() {
    // Inline Setup
    LocationService::loadTrajectory(temp_trajectory_file);
    double x, y;

    // Exercise SUT - Get coordinates at timestamp 100ms (second point in trajectory)
    LocationService::getCoordinates(x, y, std::chrono::milliseconds(100));

    // Result Verification - At 100ms, vehicle should be at approximately (1/6, 1/6) of the way
    // from (0,0) to (100,100), so around (16.67, 16.67)
    assert_true(x >= 0.0 && x <= 100.0, "Returned x coordinate should be within trajectory bounds");
    assert_true(y >= 0.0 && y <= 100.0, "Returned y coordinate should be within trajectory bounds");
    
    // More specific check - at 100ms we should be at (100/600)*100 = 16.67
    double expected = (100.0 / 60000.0) * 100.0; // ~0.167
    assert_true(x >= expected - 1.0 && x <= expected + 1.0, "X coordinate should be close to expected position");
    assert_true(y >= expected - 1.0 && y <= expected + 1.0, "Y coordinate should be close to expected position");
}

void LocationServiceTest::test_get_coordinates_at_runtime() {
    // Inline Setup
    LocationService::loadTrajectory(temp_trajectory_file);
    double x, y;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Exercise SUT
    LocationService::getCurrentCoordinates(x, y);

    // Result Verification - Check that we get valid coordinates from trajectory
    assert_true(x >= 0.0 && x <= 100.0, "X coordinate should be in the trajectory range");
    assert_true(y >= 0.0 && y <= 100.0, "Y coordinate should be in the trajectory range");
}

void LocationServiceTest::test_get_trajectory_duration() {
    // Inline Setup
    LocationService::loadTrajectory(temp_trajectory_file);

    // Exercise SUT
    auto duration = LocationService::getTrajectoryDuration();

    // Result Verification - Our test trajectory goes from 0ms to 60000ms
    assert_equal(static_cast<long long>(60000), duration.count(), "Returned trajectory duration does not match the test file duration");
}

int main() {
    LocationServiceTest test;
    test.run();

    return 0;
}