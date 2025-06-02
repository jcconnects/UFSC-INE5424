#ifndef LOCATION_SERVICE_H
#define LOCATION_SERVICE_H

// Enhanced LocationService that reads trajectory data from CSV files
// as described in IdeaP5Enzo.md. Falls back to manual coordinates if no trajectory file is loaded.

#include <mutex>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <chrono>
#include <algorithm>

struct Coordinates {
    double latitude;
    double longitude;
    double radius;
};

struct TrajectoryPoint {
    std::chrono::milliseconds timestamp;
    double latitude;
    double longitude;
    
    TrajectoryPoint(long long ts_ms, double lat, double lon) : timestamp(ts_ms), latitude(lat), longitude(lon) {}
};

class LocationService {
    public:
        
        // Load trajectory from CSV file for time-based positioning
        static bool loadTrajectory(const std::string& csv_filename);
        
        // Get coordinates at specific timestamp (reads from trajectory if loaded)
        static void getCoordinates(double& lat, double& lon, std::chrono::milliseconds timestamp = std::chrono::milliseconds::zero());
        
        // Backward compatibility: get coordinates at current system time
        static void getCurrentCoordinates(double& lat, double& lon);
        
        // Set manual coordinates (used when no trajectory is loaded)
        static void setCurrentCoordinates(double lat, double lon);
        
        // Check if trajectory is loaded
        static bool hasTrajectory();
        
        // Get trajectory duration
        static std::chrono::milliseconds getTrajectoryDuration();

    private:
        
        static bool loadTrajectoryFromCSV(const std::string& filename);
        
        static void getCoordinatesAtTime(std::chrono::milliseconds timestamp, double& lat, double& lon);

    private:
        static std::vector<TrajectoryPoint> _trajectory;
        static double _manual_latitude, _manual_longitude;
        static std::mutex _mutex;
        static std::chrono::milliseconds _start_time;
};

std::mutex LocationService::_mutex;
std::vector<TrajectoryPoint> LocationService::_trajectory;
double LocationService::_manual_latitude = 0;
double LocationService::_manual_longitude = 0;
std::chrono::milliseconds LocationService::_start_time = std::chrono::milliseconds::zero();

bool LocationService::loadTrajectory(const std::string& csv_filename) {
    std::lock_guard<std::mutex> lock(_mutex);
    if(loadTrajectoryFromCSV(csv_filename)){
        _start_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
        return true;
    };
    return false;
}

void LocationService::getCoordinates(double& lat, double& lon, std::chrono::milliseconds timestamp) {
    std::lock_guard<std::mutex> lock(_mutex);
            
    if (_trajectory.empty() || timestamp == std::chrono::milliseconds::zero()) {
        // Use manual coordinates if no trajectory or no timestamp provided
        lat = _manual_latitude;
        lon = _manual_longitude;
        return;
    }
    
    getCoordinatesAtTime(timestamp, lat, lon);
}

void LocationService::getCurrentCoordinates(double& lat, double& lon) {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    getCoordinates(lat, lon, now - _start_time);
}

void LocationService::setCurrentCoordinates(double lat, double lon) {
    std::lock_guard<std::mutex> lock(_mutex);
    _manual_latitude = lat;
    _manual_longitude = lon;
}

bool LocationService::hasTrajectory() {
    std::lock_guard<std::mutex> lock(_mutex);
    return !_trajectory.empty();
}

std::chrono::milliseconds LocationService::getTrajectoryDuration() {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_trajectory.empty()) return std::chrono::milliseconds::zero();
    return _trajectory.back().timestamp - _trajectory.front().timestamp;
}

bool LocationService::loadTrajectoryFromCSV(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    _trajectory.clear();
    std::string line;
    
    // Skip header if present
    if (std::getline(file, line) && line.find("timestamp") != std::string::npos) {
        // Header detected, continue to next line
    } else {
        // No header, reset to beginning
        file.clear();
        file.seekg(0);
    }
    
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        std::stringstream ss(line);
        std::string cell;
        std::vector<std::string> values;
        
        // Parse CSV line
        while (std::getline(ss, cell, ',')) {
            values.push_back(cell);
        }
        
        if (values.size() >= 3) {
            try {
                long long timestamp_ms = std::stoll(values[0]);
                double latitude = std::stod(values[1]);
                double longitude = std::stod(values[2]);
                
                _trajectory.emplace_back(timestamp_ms, latitude, longitude);
            } catch (const std::exception&) {
                // Skip malformed lines
                continue;
            }
        }
    }
    
    // Sort trajectory by timestamp
    std::sort(_trajectory.begin(), _trajectory.end(), 
            [](const TrajectoryPoint& a, const TrajectoryPoint& b) {
                return a.timestamp < b.timestamp;
            });
    
    return !_trajectory.empty();
}

void LocationService::getCoordinatesAtTime(std::chrono::milliseconds timestamp, double& lat, double& lon)  {
    if (_trajectory.empty()) {
        lat = _manual_latitude;
        lon = _manual_longitude;
        return;
    }
    
    // Find the first timestamp that is higher than the given one
    auto it = std::upper_bound(_trajectory.begin(), _trajectory.end(), timestamp,
                            [](std::chrono::milliseconds ts, const TrajectoryPoint& point) {
                                return ts < point.timestamp;
                            });
    
    if (it == _trajectory.begin()) {
        // Timestamp is before trajectory start - use first point
        lat = _trajectory.front().latitude;
        lon = _trajectory.front().longitude;
    } else if (it == _trajectory.end()) {
        // Timestamp is after trajectory end - use last point
        lat = _trajectory.back().latitude;
        lon = _trajectory.back().longitude;
    } else {
        // Interpolate between two points for smoother movement
        auto curr = it;
        auto prev = it - 1;
        
        auto dt_total = curr->timestamp - prev->timestamp;
        auto dt_elapsed = timestamp - prev->timestamp;
        
        if (dt_total.count() == 0) {
            // Same timestamp, use current point
            lat = curr->latitude;
            lon = curr->longitude;
        } else {
            // Linear interpolation
            double ratio = static_cast<double>(dt_elapsed.count()) / dt_total.count();
            lat = prev->latitude + ratio * (curr->latitude - prev->latitude);
            lon = prev->longitude + ratio * (curr->longitude - prev->longitude);
        }
    }
}

#endif // LOCATION_SERVICE_H 