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

    // Result Verification
    assert_equal(-27.59101730, lat, "Returned latitude does not match the latitude from the requested timestamp of the trajectory file");
    assert_equal(-48.54810875, lon, "Returned longitude does not match the longitude from the requested timestamp of the trajectory file");
}

void LocationServiceTest::test_get_coordinates_at_runtime() {
    // Inline Setup
    LocationService::loadTrajectory("tests/logs/trajectories/vehicle_1_trajectory.csv");
    double lat, lon;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Exercise SUT
    LocationService::getCurrentCoordinates(lat, lon);

    // Result Verification - Check that we get valid coordinates from trajectory
    // Since this depends on precise timing, just verify the coordinates are in the expected range
    assert_true(lat >= -27.6 && lat <= -27.58, "Latitude should be in the trajectory range");
    assert_true(lon >= -48.56 && lon <= -48.54, "Longitude should be in the trajectory range");
}

void LocationServiceTest::test_get_trajectory_duration() {
    // Inline Setup
    LocationService::loadTrajectory("tests/logs/trajectories/vehicle_1_trajectory.csv");

    // Exercise SUT
    auto duration = LocationService::getTrajectoryDuration();

    // Result Verification
    assert_equal(30000, duration.count(), "Returned trajectory duration does not match the loaded file duration");
}

int main() {
    LocationServiceTest test;
    test.run();

    return 0;
}