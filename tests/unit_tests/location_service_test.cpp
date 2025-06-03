#include "../testcase.h"
#include "api/framework/location_service.h"
#include <thread>
#include <chrono>

class LocationServiceTest : public TestCase {
    public:
        LocationServiceTest();
        ~LocationServiceTest() = default;

        void setUp() override {};
        void tearDown() override {};

        /* TESTS */
        void test_load_trajectory();
        void test_get_specific_coordinates();
        void test_get_coordinates_at_runtime();
        void test_get_trajectory_duration();
};

LocationServiceTest::LocationServiceTest() {
    DEFINE_TEST(test_load_trajectory);
    DEFINE_TEST(test_get_specific_coordinates);
    DEFINE_TEST(test_get_coordinates_at_runtime);
    DEFINE_TEST(test_get_trajectory_duration);
}

void LocationServiceTest::test_load_trajectory() {
    // Exercise SUT
    assert_true(LocationService::loadTrajectory("tests/logs/trajectories/vehicle_1_trajectory.csv"), "LocationService failed to load trajectory file");
    
}

void LocationServiceTest::test_get_specific_coordinates() {
    // Inline Setup
    LocationService::loadTrajectory("tests/logs/trajectories/vehicle_1_trajectory.csv");
    double lat, lon;

    // Exercise SUT - Get coordinates at timestamp 100ms (second point in trajectory)
    LocationService::getCoordinates(lat, lon, std::chrono::milliseconds(100));

    // Result Verification - Since trajectory is randomized, verify coordinates are within map bounds
    // Map bounds from trajectory_generator_map_1.py: lat [-27.5969, -27.5869], lon [-48.5482, -48.5382]
    assert_true(lat >= -27.597 && lat <= -27.586, "Returned latitude should be within map bounds");
    assert_true(lon >= -48.549 && lon <= -48.537, "Returned longitude should be within map bounds");
}

void LocationServiceTest::test_get_coordinates_at_runtime() {
    // Inline Setup
    LocationService::loadTrajectory("tests/logs/trajectories/vehicle_1_trajectory.csv");
    double lat, lon;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Exercise SUT
    LocationService::getCurrentCoordinates(lat, lon);

    // Result Verification - Check that we get valid coordinates from trajectory
    // Use map bounds from trajectory_generator_map_1.py for verification
    assert_true(lat >= -27.598 && lat <= -27.585, "Latitude should be in the trajectory range");
    assert_true(lon >= -48.550 && lon <= -48.535, "Longitude should be in the trajectory range");
}

void LocationServiceTest::test_get_trajectory_duration() {
    // Inline Setup
    LocationService::loadTrajectory("tests/logs/trajectories/vehicle_1_trajectory.csv");

    // Exercise SUT
    auto duration = LocationService::getTrajectoryDuration();

    // Result Verification
    assert_equal(static_cast<long long>(30000), duration.count(), "Returned trajectory duration does not match the loaded file duration");
}

int main() {
    LocationServiceTest test;
    test.run();

    return 0;
}