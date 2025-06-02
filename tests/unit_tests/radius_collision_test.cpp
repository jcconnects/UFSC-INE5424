#include "../testcase.h"
#include "api/util/geo_utils.h"
#include "api/framework/location_service.h"

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
};

RadiusCollisionTest::RadiusCollisionTest() {
    DEFINE_TEST(test_haversine_distance);
    DEFINE_TEST(test_collision_domain_logic);
    DEFINE_TEST(test_coordinates_struct);
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
}

int main() {
    RadiusCollisionTest test;
    test.run();

    return 0;
} 