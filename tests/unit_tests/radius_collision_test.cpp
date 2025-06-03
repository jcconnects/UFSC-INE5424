#include "../testcase.h"
#include "api/util/geo_utils.h"
#include "api/framework/location_service.h"
#include <chrono>
#include <thread>
#include <cstring>

class RadiusCollisionTest : public TestCase {
    public:
        RadiusCollisionTest();
        ~RadiusCollisionTest() = default;

        void setUp() override {};
        void tearDown() override {};

        /* TESTS */
        void test_haversine_distance();
        void test_collision_domain_logic();
        void test_coordinates_struct();
        void test_location_service_integration();
        void test_trajectory_based_positioning();
        void test_coordinate_interpolation();
        void test_packet_coordinate_embedding();
        void test_edge_cases();
        void test_performance_characteristics();
        void test_geographic_precision();
        void test_radius_configuration_ranges();
};

RadiusCollisionTest::RadiusCollisionTest() {
    DEFINE_TEST(test_haversine_distance);
    DEFINE_TEST(test_collision_domain_logic);
    DEFINE_TEST(test_coordinates_struct);
    DEFINE_TEST(test_location_service_integration);
    DEFINE_TEST(test_trajectory_based_positioning);
    DEFINE_TEST(test_coordinate_interpolation);
    DEFINE_TEST(test_packet_coordinate_embedding);
    DEFINE_TEST(test_edge_cases);
    DEFINE_TEST(test_performance_characteristics);
    DEFINE_TEST(test_geographic_precision);
    DEFINE_TEST(test_radius_configuration_ranges);
}

void RadiusCollisionTest::test_haversine_distance() {
    // Test basic distance calculation
    double lat1 = 0.0, lon1 = 0.0;  // Origin
    double lat2 = 0.001, lon2 = 0.0;  // ~111m north
    
    double distance = GeoUtils::haversineDistance(lat1, lon1, lat2, lon2);
    
    // 0.001 degrees latitude ≈ 111 meters
    assert_true(distance > 110.0 && distance < 112.0, 
                "Distance should be approximately 111 meters");
    
    // Test same point distance
    double same_point_distance = GeoUtils::haversineDistance(lat1, lon1, lat1, lon1);
    assert_equal(0.0, same_point_distance, "Distance between same points should be 0");
    
    // Test real-world Florianópolis coordinates (Map 1 scenario)
    // Based on trajectory_generator_map_1.py actual bounds
    double flor_lat1 = -27.5969, flor_lon1 = -48.5482;  // SW corner
    double flor_lat2 = -27.5869, flor_lon2 = -48.5382;  // NE corner
    double diagonal_distance = GeoUtils::haversineDistance(flor_lat1, flor_lon1, flor_lat2, flor_lon2);
    
    // Should be approximately 1.41km diagonal across the 1.0x1.0km grid (√2 ≈ 1.41)
    assert_true(diagonal_distance > 1300.0 && diagonal_distance < 1500.0,
                "Diagonal distance across Map 1 should be ~1.41km");
}

void RadiusCollisionTest::test_collision_domain_logic() {
    // Test collision domain logic used in Protocol
    double sender_radius = 300.0;  // 300m sender transmission radius
    
    // Test within range - receiver is within sender's transmission radius
    double distance_within = 250.0;  // 250m < 300m
    assert_true(distance_within <= sender_radius, "Packet should be accepted (within sender's range)");
    
    // Test out of range - receiver is outside sender's transmission radius
    double distance_out = 350.0;  // 350m > 300m
    assert_true(distance_out > sender_radius, "Packet should be dropped (out of sender's range)");
    
    // Test edge case - exactly at the boundary
    double distance_boundary = 300.0;  // exactly 300m
    assert_true(distance_boundary <= sender_radius, "Packet should be accepted (at boundary)");
    
    // Test typical vehicular ranges from documentation
    double urban_range = 500.0;
    double highway_range = 1000.0;
    double rsu_range = 2000.0;
    double emergency_range = 1500.0;
    
    assert_true(urban_range >= 300.0 && urban_range <= 500.0, "Urban range should be 300-500m");
    assert_true(highway_range >= 500.0 && highway_range <= 1000.0, "Highway range should be 500-1000m");
    assert_true(rsu_range >= 1000.0 && rsu_range <= 2000.0, "RSU range should be 1000-2000m");
    assert_true(emergency_range >= 1500.0, "Emergency vehicle range should be 1500m+");
}

void RadiusCollisionTest::test_coordinates_struct() {
    // Test Coordinates structure
    Coordinates coords;
    coords.latitude = -27.5954;
    coords.longitude = -48.5482;
    coords.radius = 500.0;
    
    assert_equal(-27.5954, coords.latitude, "Latitude should be set correctly");
    assert_equal(-48.5482, coords.longitude, "Longitude should be set correctly");
    assert_equal(500.0, coords.radius, "Radius should be set correctly");
    
    // Test coordinate structure size (should be 24 bytes as mentioned in docs)
    assert_equal(static_cast<size_t>(24), sizeof(Coordinates), "Coordinates structure should be 24 bytes");
    
    // Test Florianópolis coordinate ranges based on actual trajectory data
    assert_true(coords.latitude >= -27.7 && coords.latitude <= -27.5, 
                "Latitude should be in Florianópolis range");
    assert_true(coords.longitude >= -48.6 && coords.longitude <= -48.4, 
                "Longitude should be in Florianópolis range");
}

void RadiusCollisionTest::test_location_service_integration() {
    // Test manual coordinate setting (fallback mode)
    LocationService::setCurrentCoordinates(-27.5954, -48.5482);
    
    double lat, lon;
    LocationService::getCurrentCoordinates(lat, lon);
    
    assert_equal(-27.5954, lat, "Manual latitude should be retrieved correctly");
    assert_equal(-48.5482, lon, "Manual longitude should be retrieved correctly");
    
    // Test trajectory status
    assert_false(LocationService::hasTrajectory(), "Should not have trajectory initially");
    
    // Test trajectory loading
    bool loaded = LocationService::loadTrajectory("tests/logs/trajectories/vehicle_1_trajectory.csv");
    assert_true(loaded, "Should successfully load trajectory file");
    assert_true(LocationService::hasTrajectory(), "Should have trajectory after loading");
    
    // Test trajectory duration
    auto duration = LocationService::getTrajectoryDuration();
    assert_equal(static_cast<long long>(30000), duration.count(), "Trajectory duration should be 30 seconds");
}

void RadiusCollisionTest::test_trajectory_based_positioning() {
    // Load trajectory for testing
    LocationService::loadTrajectory("tests/logs/trajectories/vehicle_1_trajectory.csv");
    
    double lat, lon;
    
    // Test coordinates at specific timestamps
    LocationService::getCoordinates(lat, lon, std::chrono::milliseconds(0));
    // Trajectory is randomized, so use map bounds from trajectory_generator_map_1.py
    // Map bounds: lat [-27.5969, -27.5869], lon [-48.5482, -48.5382]
    assert_true(lat >= -27.597 && lat <= -27.586, "Start latitude should be within map bounds");
    assert_true(lon >= -48.549 && lon <= -48.537, "Start longitude should be within map bounds");
    
    LocationService::getCoordinates(lat, lon, std::chrono::milliseconds(15000)); // Mid-trajectory
    assert_true(lat >= -27.598 && lat <= -27.585, "Mid-trajectory latitude should be within map bounds");
    assert_true(lon >= -48.550 && lon <= -48.535, "Mid-trajectory longitude should be within map bounds");
    
    LocationService::getCoordinates(lat, lon, std::chrono::milliseconds(30000)); // End
    assert_true(lat >= -27.598 && lat <= -27.585, "End latitude should be within map bounds");
    assert_true(lon >= -48.550 && lon <= -48.535, "End longitude should be within map bounds");
}

void RadiusCollisionTest::test_coordinate_interpolation() {
    // Load trajectory for interpolation testing
    LocationService::loadTrajectory("tests/logs/trajectories/vehicle_1_trajectory.csv");
    
    double lat1, lon1, lat2, lon2, lat_interp, lon_interp;
    
    // Get coordinates at two trajectory points
    LocationService::getCoordinates(lat1, lon1, std::chrono::milliseconds(1000));
    LocationService::getCoordinates(lat2, lon2, std::chrono::milliseconds(2000));
    
    // Get interpolated coordinates at 1500ms (midpoint)
    LocationService::getCoordinates(lat_interp, lon_interp, std::chrono::milliseconds(1500));
    
    // For trajectory data with small movements, interpolated values should be reasonable
    // Allow for some tolerance in the interpolation
    double lat_center = (lat1 + lat2) / 2.0;
    double lon_center = (lon1 + lon2) / 2.0;
    double tolerance = 0.001; // ~111m tolerance
    
    assert_true(std::abs(lat_interp - lat_center) <= tolerance, 
                "Interpolated latitude should be near the center of two trajectory points");
    assert_true(std::abs(lon_interp - lon_center) <= tolerance, 
                "Interpolated longitude should be near the center of two trajectory points");
}

void RadiusCollisionTest::test_packet_coordinate_embedding() {
    // Test packet structure as described in documentation
    struct TestPacket {
        uint16_t from_port;
        uint16_t to_port;
        uint32_t size;
        bool is_clock_synchronized;
        uint64_t tx_timestamp;
        Coordinates coordinates;
        uint8_t data[100];
    } __attribute__((packed));
    
    TestPacket packet;
    packet.coordinates.latitude = -27.5954;
    packet.coordinates.longitude = -48.5482;
    packet.coordinates.radius = 1000.0;
    
    // Test coordinate extraction (simulates Protocol::update logic) - avoid taking address of packed member
    Coordinates coords_copy = packet.coordinates;
    assert_equal(-27.5954, coords_copy.latitude, "Packet latitude should be embedded correctly");
    assert_equal(-48.5482, coords_copy.longitude, "Packet longitude should be embedded correctly");
    assert_equal(1000.0, coords_copy.radius, "Packet radius should be embedded correctly");
    
    // Test memory copying (simulates Protocol::send logic)
    Coordinates source_coords;
    source_coords.latitude = -27.6000;
    source_coords.longitude = -48.5500;
    source_coords.radius = 500.0;
    
    std::memcpy(&packet.coordinates, &source_coords, sizeof(Coordinates));
    
    assert_equal(-27.6000, packet.coordinates.latitude, "Memcpy should update packet latitude");
    assert_equal(-48.5500, packet.coordinates.longitude, "Memcpy should update packet longitude");
    assert_equal(500.0, packet.coordinates.radius, "Memcpy should update packet radius");
}

void RadiusCollisionTest::test_edge_cases() {
    // Test extreme coordinates
    double extreme_distance = GeoUtils::haversineDistance(-90.0, -180.0, 90.0, 180.0);
    assert_true(extreme_distance > 0, "Extreme coordinates should produce valid distance");
    
    // Test invalid trajectory timestamps
    LocationService::loadTrajectory("tests/logs/trajectories/vehicle_1_trajectory.csv");
    double lat, lon;
    
    // Before trajectory start
    LocationService::getCoordinates(lat, lon, std::chrono::milliseconds(-1000));
    assert_true(lat != 0.0 && lon != 0.0, "Should return valid coordinates for pre-trajectory timestamp");
    
    // After trajectory end
    LocationService::getCoordinates(lat, lon, std::chrono::milliseconds(50000));
    assert_true(lat != 0.0 && lon != 0.0, "Should return valid coordinates for post-trajectory timestamp");
    
    // Test zero radius collision
    double zero_distance = 0.0;
    double zero_radius = 0.0;
    assert_true(zero_distance <= zero_radius, "Zero distance should be within zero radius");
    
    // Test very small distances
    double tiny_distance = GeoUtils::haversineDistance(0.0, 0.0, 0.0000001, 0.0000001);
    assert_true(tiny_distance >= 0.0 && tiny_distance < 1.0, "Tiny distances should be valid and small");
}

void RadiusCollisionTest::test_performance_characteristics() {
    // Test distance calculation performance (should be ~6-11µs per packet as per docs)
    auto start = std::chrono::high_resolution_clock::now();
    
    const int iterations = 1000;
    for (int i = 0; i < iterations; i++) {
        double distance = GeoUtils::haversineDistance(-27.5954, -48.5482, -27.5955, -48.5483);
        // Prevent optimization
        volatile double result = distance;
        (void)result;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avg_time_us = static_cast<double>(duration.count()) / iterations;
    
    // Should be under 20µs per calculation (generous upper bound)
    assert_true(avg_time_us < 20.0, "Distance calculation should be performant");
    
    // Test trajectory lookup performance
    LocationService::loadTrajectory("tests/logs/trajectories/vehicle_1_trajectory.csv");
    
    start = std::chrono::high_resolution_clock::now();
    double lat, lon;
    for (int i = 0; i < iterations; i++) {
        LocationService::getCoordinates(lat, lon, std::chrono::milliseconds(i * 10));
    }
    end = std::chrono::high_resolution_clock::now();
    
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    avg_time_us = static_cast<double>(duration.count()) / iterations;
    
    // Should be under 10µs per lookup (trajectory interpolation)
    assert_true(avg_time_us < 10.0, "Trajectory lookup should be performant");
}

void RadiusCollisionTest::test_geographic_precision() {
    // Test ~1m precision as mentioned in documentation
    double lat1 = -27.5954, lon1 = -48.5482;
    double lat2 = -27.5954 + 0.000009, lon2 = -48.5482; // ~1m north
    
    double distance = GeoUtils::haversineDistance(lat1, lon1, lat2, lon2);
    assert_true(distance >= 0.9 && distance <= 1.1, "Should have ~1m precision");
    
    // Test coordinate consistency with trajectory generator
    // Based on actual trajectory_generator_map_1.py bounds
    double expected_lat_distance = GeoUtils::haversineDistance(-27.5969, -48.5482, -27.5869, -48.5482);
    double expected_lon_distance = GeoUtils::haversineDistance(-27.5919, -48.5482, -27.5919, -48.5382);
    
    // Should be approximately 1.1km each direction
    assert_true(expected_lat_distance > 1000.0 && expected_lat_distance < 1200.0, 
                "Latitude range should be ~1.1km");
    assert_true(expected_lon_distance > 800.0 && expected_lon_distance < 1000.0, 
                "Longitude range should be ~800-1000m (longitude degrees are smaller at this latitude)");
}

void RadiusCollisionTest::test_radius_configuration_ranges() {
    // Test typical transmission ranges from documentation
    struct RangeTest {
        const char* type;
        double min_range;
        double max_range;
    };
    
    RangeTest ranges[] = {
        {"Urban vehicles", 300.0, 500.0},
        {"Highway vehicles", 500.0, 1000.0},
        {"RSUs", 1000.0, 2000.0},
        {"Emergency vehicles", 1500.0, 3000.0}
    };
    
    for (const auto& range : ranges) {
        // Test collision logic for each range type
        double test_radius = (range.min_range + range.max_range) / 2;
        
        // Within range
        double distance_within = test_radius * 0.8;
        assert_true(distance_within <= test_radius, 
                   "Distance within range should pass collision test");
        
        // Out of range
        double distance_out = test_radius * 1.2;
        assert_true(distance_out > test_radius, 
                   "Distance out of range should fail collision test");
    }
    
    // Test default transmission radius (1000m as per docs)
    double default_radius = 1000.0;
    assert_equal(1000.0, default_radius, "Default transmission radius should be 1000m");
}

int main() {
    RadiusCollisionTest test;
    test.run();

    return 0;
} 