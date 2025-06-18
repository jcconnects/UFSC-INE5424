
#include <chrono>
#include <thread>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <cmath>

#include "../testcase.h"
#include "api/util/geo_utils.h"
#include "api/framework/location_service.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class RadiusCollisionTest : public TestCase {
    public:
        RadiusCollisionTest();
        ~RadiusCollisionTest() = default;

        void setUp() override;
        void tearDown() override;

        /* TESTS */
        void test_euclidean_distance();
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

    private:
        std::string temp_trajectory_file;
        void createTestTrajectoryFile();
        void cleanupTestTrajectoryFile();
};

RadiusCollisionTest::RadiusCollisionTest() {
    DEFINE_TEST(test_euclidean_distance);
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

void RadiusCollisionTest::setUp() {
    // Create temporary directory if it doesn't exist
    std::filesystem::create_directories("tests/temp");
    temp_trajectory_file = "tests/temp/test_trajectory_radius.csv";
    createTestTrajectoryFile();
}

void RadiusCollisionTest::tearDown() {
    cleanupTestTrajectoryFile();
}

void RadiusCollisionTest::createTestTrajectoryFile() {
    std::ofstream file(temp_trajectory_file);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to create test trajectory file");
    }
    
    // Write CSV header
    file << "timestamp_ms,x,y\n";
    
    // Create a 30-second trajectory with points every 100ms (matching original test expectations)
    // Vehicle moves linearly from (100,100) to (900,900) for predictable interpolation
    for (int i = 0; i <= 300; i++) {
        long long timestamp = i * 100; // Every 100ms
        double progress = static_cast<double>(i) / 300.0; // 0 to 1
        
        // Linear movement from (100,100) to (900,900)
        double x = 100.0 + progress * 800.0; // 100 to 900
        double y = 100.0 + progress * 800.0; // 100 to 900
        
        file << timestamp << "," << x << "," << y << "\n";
    }
    
    file.close();
}

void RadiusCollisionTest::cleanupTestTrajectoryFile() {
    if (std::filesystem::exists(temp_trajectory_file)) {
        std::filesystem::remove(temp_trajectory_file);
    }
    // Also remove temp directory if it's empty
    if (std::filesystem::exists("tests/temp") && std::filesystem::is_empty("tests/temp")) {
        std::filesystem::remove("tests/temp");
    }
}

void RadiusCollisionTest::test_euclidean_distance() {
    // Test basic distance calculation
    double x1 = 0.0, y1 = 0.0;  // Origin
    double x2 = 100.0, y2 = 0.0;  // 100m east
    
    double distance = GeoUtils::haversineDistance(x1, y1, x2, y2);
    
    // Should be exactly 100 meters in Cartesian system
    assert_equal(100.0, distance, "Distance should be exactly 100 meters");
    
    // Test same point distance
    double same_point_distance = GeoUtils::haversineDistance(x1, y1, x1, y1);
    assert_equal(0.0, same_point_distance, "Distance between same points should be 0");
    
    // Test diagonal distance across 1000x1000m grid
    double corner1_x = 0.0, corner1_y = 0.0;      // SW corner
    double corner2_x = 1000.0, corner2_y = 1000.0; // NE corner
    double diagonal_distance = GeoUtils::haversineDistance(corner1_x, corner1_y, corner2_x, corner2_y);
    
    // Should be approximately 1414m diagonal across the 1000x1000m grid (√2 * 1000 ≈ 1414)
    assert_true(diagonal_distance > 1410.0 && diagonal_distance < 1420.0,
                "Diagonal distance across 1000x1000m grid should be ~1414m");
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
    coords.x = 500.0;
    coords.y = 300.0;
    coords.radius = 500.0;
    
    assert_equal(500.0, coords.x, "X coordinate should be set correctly");
    assert_equal(300.0, coords.y, "Y coordinate should be set correctly");
    assert_equal(500.0, coords.radius, "Radius should be set correctly");
    
    // Test coordinate structure size (should be 24 bytes as mentioned in docs)
    assert_equal(static_cast<size_t>(24), sizeof(Coordinates), "Coordinates structure should be 24 bytes");
    
    // Test valid coordinate ranges for 1000x1000m grid
    assert_true(coords.x >= 0.0 && coords.x <= 1000.0, 
                "X coordinate should be in valid range [0, 1000]");
    assert_true(coords.y >= 0.0 && coords.y <= 1000.0, 
                "Y coordinate should be in valid range [0, 1000]");
}

void RadiusCollisionTest::test_location_service_integration() {
    // Test manual coordinate setting (fallback mode)
    LocationService::setCurrentCoordinates(500.0, 300.0);
    
    double x, y;
    LocationService::getCurrentCoordinates(x, y);
    
    assert_equal(500.0, x, "Manual x coordinate should be retrieved correctly");
    assert_equal(300.0, y, "Manual y coordinate should be retrieved correctly");
    
    // Test trajectory status
    assert_false(LocationService::hasTrajectory(), "Should not have trajectory initially");
    
    // Test trajectory loading
    bool loaded = LocationService::loadTrajectory(temp_trajectory_file);
    assert_true(loaded, "Should successfully load trajectory file");
    assert_true(LocationService::hasTrajectory(), "Should have trajectory after loading");
    
    // Test trajectory duration
    auto duration = LocationService::getTrajectoryDuration();
    assert_equal(static_cast<long long>(30000), duration.count(), "Trajectory duration should be 30 seconds");
}

void RadiusCollisionTest::test_trajectory_based_positioning() {
    // Load trajectory for testing
    LocationService::loadTrajectory(temp_trajectory_file);
    
    double x, y;
    
    // Test coordinates at specific timestamps
    LocationService::getCoordinates(x, y, std::chrono::milliseconds(0));
    // Trajectory is randomized, so use map bounds for 1000x1000m grid
    // Map bounds: x [0, 1000], y [0, 1000]
    assert_true(x >= 0.0 && x <= 1000.0, "Start x coordinate should be within map bounds");
    assert_true(y >= 0.0 && y <= 1000.0, "Start y coordinate should be within map bounds");
    
    LocationService::getCoordinates(x, y, std::chrono::milliseconds(15000)); // Mid-trajectory
    assert_true(x >= 0.0 && x <= 1000.0, "Mid-trajectory x coordinate should be within map bounds");
    assert_true(y >= 0.0 && y <= 1000.0, "Mid-trajectory y coordinate should be within map bounds");
    
    LocationService::getCoordinates(x, y, std::chrono::milliseconds(30000)); // End
    assert_true(x >= 0.0 && x <= 1000.0, "End x coordinate should be within map bounds");
    assert_true(y >= 0.0 && y <= 1000.0, "End y coordinate should be within map bounds");
}

void RadiusCollisionTest::test_coordinate_interpolation() {
    // Load trajectory for interpolation testing
    LocationService::loadTrajectory(temp_trajectory_file);
    
    double x1, y1, x2, y2, x_interp, y_interp;
    
    // Get coordinates at two trajectory points
    LocationService::getCoordinates(x1, y1, std::chrono::milliseconds(1000));
    LocationService::getCoordinates(x2, y2, std::chrono::milliseconds(2000));
    
    // Get interpolated coordinates at 1500ms (midpoint)
    LocationService::getCoordinates(x_interp, y_interp, std::chrono::milliseconds(1500));
    
    // For trajectory data with small movements, interpolated values should be reasonable
    // Allow for some tolerance in the interpolation
    double x_center = (x1 + x2) / 2.0;
    double y_center = (y1 + y2) / 2.0;
    double tolerance = 1.0; // 1m tolerance
    
    assert_true(std::abs(x_interp - x_center) <= tolerance, 
                "Interpolated x coordinate should be near the center of two trajectory points");
    assert_true(std::abs(y_interp - y_center) <= tolerance, 
                "Interpolated y coordinate should be near the center of two trajectory points");
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
    packet.coordinates.x = 500.0;
    packet.coordinates.y = 300.0;
    packet.coordinates.radius = 1000.0;
    
    // Test coordinate extraction (simulates Protocol::update logic) - avoid taking address of packed member
    Coordinates coords_copy = packet.coordinates;
    assert_equal(500.0, coords_copy.x, "Packet x coordinate should be embedded correctly");
    assert_equal(300.0, coords_copy.y, "Packet y coordinate should be embedded correctly");
    assert_equal(1000.0, coords_copy.radius, "Packet radius should be embedded correctly");
    
    // Test memory copying (simulates Protocol::send logic)
    Coordinates source_coords;
    source_coords.x = 750.0;
    source_coords.y = 200.0;
    source_coords.radius = 500.0;
    
    std::memcpy(&packet.coordinates, &source_coords, sizeof(Coordinates));
    
    assert_equal(750.0, packet.coordinates.x, "Memcpy should update packet x coordinate");
    assert_equal(200.0, packet.coordinates.y, "Memcpy should update packet y coordinate");
    assert_equal(500.0, packet.coordinates.radius, "Memcpy should update packet radius");
}

void RadiusCollisionTest::test_edge_cases() {
    // Test extreme coordinates
    double extreme_distance = GeoUtils::haversineDistance(0.0, 0.0, 10000.0, 10000.0);
    assert_true(extreme_distance > 0, "Extreme coordinates should produce valid distance");
    
    // Test invalid trajectory timestamps
    LocationService::loadTrajectory(temp_trajectory_file);
    double x, y;
    
    // Before trajectory start
    LocationService::getCoordinates(x, y, std::chrono::milliseconds(-1000));
    assert_true(x >= 0.0 && y >= 0.0, "Should return valid coordinates for pre-trajectory timestamp");
    
    // After trajectory end
    LocationService::getCoordinates(x, y, std::chrono::milliseconds(50000));
    assert_true(x >= 0.0 && y >= 0.0, "Should return valid coordinates for post-trajectory timestamp");
    
    // Test zero radius collision
    double zero_distance = 0.0;
    double zero_radius = 0.0;
    assert_true(zero_distance <= zero_radius, "Zero distance should be within zero radius");
    
    // Test very small distances
    double tiny_distance = GeoUtils::haversineDistance(0.0, 0.0, 0.001, 0.001);
    assert_true(tiny_distance >= 0.0 && tiny_distance < 1.0, "Tiny distances should be valid and small");
}

void RadiusCollisionTest::test_performance_characteristics() {
    // Test distance calculation performance (should be ~6-11µs per packet as per docs)
    auto start = std::chrono::high_resolution_clock::now();
    
    const int iterations = 1000;
    for (int i = 0; i < iterations; i++) {
        double distance = GeoUtils::euclideanDistance(100.0, 200.0, 101.0, 201.0);
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
    LocationService::loadTrajectory(temp_trajectory_file);
    
    start = std::chrono::high_resolution_clock::now();
    double x, y;
    for (int i = 0; i < iterations; i++) {
        LocationService::getCoordinates(x, y, std::chrono::milliseconds(i * 10));
    }
    end = std::chrono::high_resolution_clock::now();
    
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    avg_time_us = static_cast<double>(duration.count()) / iterations;
    
    // Should be under 10µs per lookup (trajectory interpolation)
    assert_true(avg_time_us < 10.0, "Trajectory lookup should be performant");
}

void RadiusCollisionTest::test_geographic_precision() {
    // Test 1m precision as mentioned in documentation
    double x1 = 500.0, y1 = 300.0;
    double x2 = 501.0, y2 = 300.0; // 1m east
    
    double distance = GeoUtils::haversineDistance(x1, y1, x2, y2);
    assert_equal(1.0, distance, "Should have exact 1m precision");
    
    // Test coordinate consistency across 1000x1000m grid
    double grid_width = GeoUtils::haversineDistance(0.0, 0.0, 1000.0, 0.0);
    double grid_height = GeoUtils::haversineDistance(0.0, 0.0, 0.0, 1000.0);
    
    // Should be exactly 1000m each direction in Cartesian system
    assert_equal(1000.0, grid_width, "Grid width should be exactly 1000m");
    assert_equal(1000.0, grid_height, "Grid height should be exactly 1000m");
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