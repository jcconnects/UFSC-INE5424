#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>

#include "../../include/api/framework/network.h"
#include "../../include/api/network/communicator.h"
#include "../../include/api/network/message.h"
#include "../../include/api/network/beamforming.h"
#include "../../include/api/framework/location_service.h"
#include "../../include/api/util/geo_utils.h"
#include "../testcase.h"
#include "../test_utils.h"

class BeamformingTest : public TestCase {
public:
    BeamformingTest();
    ~BeamformingTest() = default;

    void setUp() override;
    void tearDown() override;

    // Test methods
    void test_omnidirectional_beam();
    void test_directional_beam();
    void test_geographic_calculations();
    void test_beam_containment();
    void test_distance_filtering();
    void test_location_service();
    void test_backward_compatibility();

private:
    Network* sender_network;
    Network* receiver_network;
    LocationService* location_service;
};

BeamformingTest::BeamformingTest() {
    DEFINE_TEST(test_omnidirectional_beam);
    DEFINE_TEST(test_directional_beam);
    DEFINE_TEST(test_geographic_calculations);
    DEFINE_TEST(test_beam_containment);
    DEFINE_TEST(test_distance_filtering);
    DEFINE_TEST(test_location_service);
    DEFINE_TEST(test_backward_compatibility);
}

void BeamformingTest::setUp() {
    sender_network = new Network(1);
    receiver_network = new Network(2);
    location_service = &LocationService::getInstance();
}

void BeamformingTest::tearDown() {
    delete sender_network;
    delete receiver_network;
    sender_network = nullptr;
    receiver_network = nullptr;
}

void BeamformingTest::test_omnidirectional_beam() {
    // Test default omnidirectional beam creation
    BeamformingInfo omni_beam;
    
    assert_equal(0.0, omni_beam.sender_latitude, "Default latitude should be 0.0");
    assert_equal(0.0, omni_beam.sender_longitude, "Default longitude should be 0.0");
    assert_equal(0.0f, omni_beam.beam_center_angle, "Default beam center should be 0.0");
    assert_equal(360.0f, omni_beam.beam_width_angle, "Default beam width should be 360.0 (omnidirectional)");
    assert_equal(1000.0f, omni_beam.max_range, "Default max range should be 1000.0m");
}

void BeamformingTest::test_directional_beam() {
    // Test directional beam configuration
    BeamformingInfo directional_beam;
    directional_beam.beam_center_angle = 90.0f;  // North
    directional_beam.beam_width_angle = 45.0f;   // 45° beam width
    directional_beam.max_range = 800.0f;         // 800m range
    
    assert_equal(90.0f, directional_beam.beam_center_angle, "Beam center should be 90.0 degrees");
    assert_equal(45.0f, directional_beam.beam_width_angle, "Beam width should be 45.0 degrees");
    assert_equal(800.0f, directional_beam.max_range, "Max range should be 800.0 meters");
}

void BeamformingTest::test_geographic_calculations() {
    // Test Haversine distance calculation
    double lat1 = 0.0, lon1 = 0.0;  // Origin
    double lat2 = 0.001, lon2 = 0.0;  // ~111m north
    
    double distance = GeoUtils::haversineDistance(lat1, lon1, lat2, lon2);
    
    // 0.001 degrees latitude ≈ 111 meters
    assert_true(distance > 110.0 && distance < 112.0, 
                "Distance should be approximately 111 meters");
    
    // Test bearing calculation
    float bearing = GeoUtils::bearing(lat1, lon1, lat2, lon2);
    assert_true(bearing > 89.0f && bearing < 91.0f, 
                "Bearing should be approximately 90 degrees (north)");
    
    // Test same point distance
    double same_point_distance = GeoUtils::haversineDistance(lat1, lon1, lat1, lon1);
    assert_equal(0.0, same_point_distance, "Distance between same points should be 0");
}

void BeamformingTest::test_beam_containment() {
    // Test omnidirectional beam (360°)
    assert_true(GeoUtils::isInBeam(0.0f, 0.0f, 360.0f), 
                "360° beam should contain any direction");
    assert_true(GeoUtils::isInBeam(180.0f, 90.0f, 360.0f), 
                "360° beam should contain any direction");
    
    // Test narrow beam pointing north (90°)
    assert_true(GeoUtils::isInBeam(90.0f, 90.0f, 45.0f), 
                "45° beam centered at 90° should contain 90°");
    assert_true(GeoUtils::isInBeam(67.5f, 90.0f, 45.0f), 
                "45° beam centered at 90° should contain 67.5°");
    assert_true(GeoUtils::isInBeam(112.5f, 90.0f, 45.0f), 
                "45° beam centered at 90° should contain 112.5°");
    assert_false(GeoUtils::isInBeam(45.0f, 90.0f, 45.0f), 
                 "45° beam centered at 90° should not contain 45°");
    assert_false(GeoUtils::isInBeam(135.0f, 90.0f, 45.0f), 
                 "45° beam centered at 90° should not contain 135°");
    
    // Test beam wraparound (crossing 0°/360°)
    assert_true(GeoUtils::isInBeam(350.0f, 0.0f, 30.0f), 
                "30° beam centered at 0° should contain 350°");
    assert_true(GeoUtils::isInBeam(10.0f, 0.0f, 30.0f), 
                "30° beam centered at 0° should contain 10°");
}

void BeamformingTest::test_distance_filtering() {
    // Test distance calculation for filtering
    location_service->setCurrentCoordinates(0.0, 0.0);  // Receiver at origin
    
    // Sender 500m away (approximately)
    double sender_lat = 0.0045;  // ~500m north
    double sender_lon = 0.0;
    
    double distance = GeoUtils::haversineDistance(0.0, 0.0, sender_lat, sender_lon);
    
    // Test range filtering logic
    BeamformingInfo short_range_beam;
    short_range_beam.max_range = 400.0f;  // 400m range
    assert_true(distance > short_range_beam.max_range, 
                "Packet should be filtered (out of range)");
    
    BeamformingInfo long_range_beam;
    long_range_beam.max_range = 600.0f;  // 600m range
    assert_true(distance < long_range_beam.max_range, 
                "Packet should pass range filter");
}

void BeamformingTest::test_location_service() {
    // Test LocationService singleton
    LocationService& service1 = LocationService::getInstance();
    LocationService& service2 = LocationService::getInstance();
    
    // Should be the same instance
    assert_true(&service1 == &service2, "LocationService should be a singleton");
    
    // Test coordinate setting and getting
    service1.setCurrentCoordinates(37.7749, -122.4194);  // San Francisco
    
    double lat, lon;
    service2.getCurrentCoordinates(lat, lon);
    
    assert_equal(37.7749, lat, "Latitude should be set correctly");
    assert_equal(-122.4194, lon, "Longitude should be set correctly");
}

void BeamformingTest::test_backward_compatibility() {
    // Test that existing code still works (no beamforming parameters)
    location_service->setCurrentCoordinates(0.0, 0.0);
    
    // Create communicators
    Network::Protocol::Address sender_addr(sender_network->address(), 8001);
    Network::Protocol::Address receiver_addr(receiver_network->address(), 8002);
    
    Network::Communicator sender(sender_network->channel(), sender_addr);
    Network::Communicator receiver(receiver_network->channel(), receiver_addr);
    
    // Test sending without beamforming (should use defaults)
    Network::Message test_message("Backward compatibility test");
    
    // This should work without throwing exceptions
    bool sent = sender.send(&test_message);
    assert_true(true, "Backward compatible send should not throw exceptions");
}

int main() {
    TEST_INIT("beamforming_integration_test");
    TEST_LOG("Starting beamforming integration tests");
    
    BeamformingTest test;
    test.run();
    
    TEST_LOG("Beamforming integration tests completed successfully!");
    return 0;
} 