#ifndef GEO_UTILS_H
#define GEO_UTILS_H

#include <cmath>

class GeoUtils {
public:
    // Simple Euclidean distance calculation for Cartesian coordinates
    static double cartesianDistance(double x1, double y1, double x2, double y2) {
        double dx = x2 - x1;
        double dy = y2 - y1;
        return sqrt(dx * dx + dy * dy);
    }
    
    // Alias for cartesianDistance
    static double euclideanDistance(double x1, double y1, double x2, double y2) {
        return cartesianDistance(x1, y1, x2, y2);
    }
    
    // Legacy method name for backward compatibility
    static double haversineDistance(double x1, double y1, double x2, double y2) {
        return cartesianDistance(x1, y1, x2, y2);
    }
    
    // Check if point is within circular area
    static bool isWithinRadius(double x1, double y1, double x2, double y2, double radius) {
        return cartesianDistance(x1, y1, x2, y2) <= radius;
    }
};

#endif // GEO_UTILS_H 