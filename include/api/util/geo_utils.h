#ifndef GEO_UTILS_H
#define GEO_UTILS_H

#include <cmath>

class GeoUtils {
public:
    static constexpr double EARTH_RADIUS_M = 6371000.0;
    
    static double haversineDistance(double lat1, double lon1, double lat2, double lon2) {
        double dlat = toRadians(lat2 - lat1);
        double dlon = toRadians(lon2 - lon1);
        double a = sin(dlat/2) * sin(dlat/2) + 
                   cos(toRadians(lat1)) * cos(toRadians(lat2)) * 
                   sin(dlon/2) * sin(dlon/2);
        return 2 * EARTH_RADIUS_M * asin(sqrt(a));
    }
    
    static float bearing(double lat1, double lon1, double lat2, double lon2) {
        double dlon = toRadians(lon2 - lon1);
        double y = sin(dlon) * cos(toRadians(lat2));
        double x = cos(toRadians(lat1)) * sin(toRadians(lat2)) - 
                   sin(toRadians(lat1)) * cos(toRadians(lat2)) * cos(dlon);
        return fmod(toDegrees(atan2(y, x)) + 360.0, 360.0);
    }
    
    static bool isInBeam(float bearing, float beam_center, float beam_width) {
        if (beam_width >= 360.0f) return true; // omnidirectional
        
        float half_width = beam_width / 2.0f;
        float diff = fmod(bearing - beam_center + 360.0f, 360.0f);
        return diff <= half_width || diff >= (360.0f - half_width);
    }

private:
    static double toRadians(double degrees) { return degrees * M_PI / 180.0; }
    static double toDegrees(double radians) { return radians * 180.0 / M_PI; }
};

#endif // GEO_UTILS_H 