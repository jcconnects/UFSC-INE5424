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

private:
    static double toRadians(double degrees) { return degrees * M_PI / 180.0; }
    static double toDegrees(double radians) { return radians * 180.0 / M_PI; }
};

#endif // GEO_UTILS_H 