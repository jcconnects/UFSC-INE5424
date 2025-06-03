#ifndef MAP_CONFIG_H
#define MAP_CONFIG_H

#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <chrono>

// Simplified configuration structures for waypoint-based trajectory generation

struct Waypoint {
    std::string name;
    double lat;
    double lon;
};

struct Route {
    std::string name;
    std::vector<std::string> waypoint_names;
};

struct RSUConfig {
    unsigned int id;
    double lat;
    double lon;
    unsigned int unit;
    std::chrono::milliseconds broadcast_period;
};

struct VehicleConfig {
    unsigned int default_count;
    double speed_kmh;
};

struct SimulationConfig {
    unsigned int duration_s;
    unsigned int update_interval_ms;
    double default_transmission_radius_m;
};

struct LoggingConfig {
    std::string trajectory_dir;
};

class MapConfig {
public:
    MapConfig(const std::string& config_file_path);
    
    // Accessors
    const std::vector<Waypoint>& waypoints() const { return _waypoints; }
    const std::vector<Route>& routes() const { return _routes; }
    const RSUConfig& rsu_config() const { return _rsu_config; }
    const VehicleConfig& vehicle_config() const { return _vehicle_config; }
    const SimulationConfig& simulation() const { return _simulation; }
    const LoggingConfig& logging() const { return _logging; }
    
    // Helper methods
    Waypoint get_waypoint(const std::string& name) const;
    Route get_route(const std::string& name) const;
    std::string get_trajectory_file_path(const std::string& entity_type, unsigned int entity_id) const;
    double get_transmission_radius() const { return _simulation.default_transmission_radius_m; }

private:
    void parse_config_file(const std::string& file_path);
    double extract_double_value(const std::string& content, const std::string& key) const;
    unsigned int extract_uint_value(const std::string& content, const std::string& key) const;
    std::string extract_string_value(const std::string& content, const std::string& key) const;
    void parse_waypoints(const std::string& content);
    void parse_routes(const std::string& content);
    
    std::vector<Waypoint> _waypoints;
    std::vector<Route> _routes;
    RSUConfig _rsu_config;
    VehicleConfig _vehicle_config;
    SimulationConfig _simulation;
    LoggingConfig _logging;
};

// Implementation
inline MapConfig::MapConfig(const std::string& config_file_path) {
    parse_config_file(config_file_path);
}

inline void MapConfig::parse_config_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open configuration file: " + file_path);
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    
    // Parse RSU config
    _rsu_config.id = extract_uint_value(content, "\"id\"");
    _rsu_config.lat = extract_double_value(content, "\"lat\"");
    _rsu_config.lon = extract_double_value(content, "\"lon\"");
    _rsu_config.unit = extract_uint_value(content, "\"unit\"");
    _rsu_config.broadcast_period = std::chrono::milliseconds(extract_uint_value(content, "broadcast_period_ms"));
    
    // Parse vehicle config
    _vehicle_config.default_count = extract_uint_value(content, "default_count");
    _vehicle_config.speed_kmh = extract_double_value(content, "speed_kmh");
    
    // Parse simulation config
    _simulation.duration_s = extract_uint_value(content, "duration_s");
    _simulation.update_interval_ms = extract_uint_value(content, "update_interval_ms");
    _simulation.default_transmission_radius_m = extract_double_value(content, "default_transmission_radius_m");
    
    // Parse logging config
    _logging.trajectory_dir = extract_string_value(content, "trajectory_dir");
    
    // Parse waypoints and routes
    parse_waypoints(content);
    parse_routes(content);
}

inline void MapConfig::parse_waypoints(const std::string& content) {
    // Simple parser for waypoints array
    size_t waypoints_start = content.find("\"waypoints\":");
    if (waypoints_start == std::string::npos) return;
    
    size_t array_start = content.find("[", waypoints_start);
    size_t array_end = content.find("]", array_start);
    
    std::string waypoints_section = content.substr(array_start + 1, array_end - array_start - 1);
    
    // Parse each waypoint object
    size_t pos = 0;
    while (pos < waypoints_section.length()) {
        size_t obj_start = waypoints_section.find("{", pos);
        if (obj_start == std::string::npos) break;
        
        size_t obj_end = waypoints_section.find("}", obj_start);
        if (obj_end == std::string::npos) break;
        
        std::string waypoint_obj = waypoints_section.substr(obj_start, obj_end - obj_start + 1);
        
        Waypoint wp;
        wp.name = extract_string_value(waypoint_obj, "name");
        wp.lat = extract_double_value(waypoint_obj, "lat");
        wp.lon = extract_double_value(waypoint_obj, "lon");
        
        _waypoints.push_back(wp);
        pos = obj_end + 1;
    }
}

inline void MapConfig::parse_routes(const std::string& content) {
    // Simple parser for routes array
    size_t routes_start = content.find("\"routes\":");
    if (routes_start == std::string::npos) return;
    
    size_t array_start = content.find("[", routes_start);
    size_t array_end = content.find("]", array_start);
    
    std::string routes_section = content.substr(array_start + 1, array_end - array_start - 1);
    
    // Parse each route object
    size_t pos = 0;
    while (pos < routes_section.length()) {
        size_t obj_start = routes_section.find("{", pos);
        if (obj_start == std::string::npos) break;
        
        size_t obj_end = routes_section.find("}", obj_start);
        if (obj_end == std::string::npos) break;
        
        std::string route_obj = routes_section.substr(obj_start, obj_end - obj_start + 1);
        
        Route route;
        route.name = extract_string_value(route_obj, "name");
        
        // Parse waypoints array within route
        size_t wp_array_start = route_obj.find("[");
        size_t wp_array_end = route_obj.find("]", wp_array_start);
        
        if (wp_array_start != std::string::npos && wp_array_end != std::string::npos) {
            std::string wp_list = route_obj.substr(wp_array_start + 1, wp_array_end - wp_array_start - 1);
            
            // Simple comma-separated parsing
            size_t wp_pos = 0;
            while (wp_pos < wp_list.length()) {
                size_t quote_start = wp_list.find("\"", wp_pos);
                if (quote_start == std::string::npos) break;
                
                size_t quote_end = wp_list.find("\"", quote_start + 1);
                if (quote_end == std::string::npos) break;
                
                std::string waypoint_name = wp_list.substr(quote_start + 1, quote_end - quote_start - 1);
                route.waypoint_names.push_back(waypoint_name);
                
                wp_pos = quote_end + 1;
            }
        }
        
        _routes.push_back(route);
        pos = obj_end + 1;
    }
}

inline double MapConfig::extract_double_value(const std::string& content, const std::string& key) const {
    std::string search_pattern = "\"" + key + "\":";
    size_t pos = content.find(search_pattern);
    if (pos == std::string::npos) {
        throw std::runtime_error("Key not found in config: " + key);
    }
    
    pos += search_pattern.length();
    
    // Skip whitespace
    while (pos < content.length() && std::isspace(content[pos])) {
        pos++;
    }
    
    // Extract the numeric value
    size_t end_pos = pos;
    while (end_pos < content.length() && 
           (std::isdigit(content[end_pos]) || content[end_pos] == '.' || content[end_pos] == '-')) {
        end_pos++;
    }
    
    if (pos == end_pos) {
        throw std::runtime_error("Invalid numeric value for key: " + key);
    }
    
    return std::stod(content.substr(pos, end_pos - pos));
}

inline unsigned int MapConfig::extract_uint_value(const std::string& content, const std::string& key) const {
    return static_cast<unsigned int>(extract_double_value(content, key));
}

inline std::string MapConfig::extract_string_value(const std::string& content, const std::string& key) const {
    std::string search_pattern = "\"" + key + "\":";
    size_t pos = content.find(search_pattern);
    if (pos == std::string::npos) {
        throw std::runtime_error("Key not found in config: " + key);
    }
    
    pos += search_pattern.length();
    
    // Skip whitespace and find opening quote
    while (pos < content.length() && (std::isspace(content[pos]) || content[pos] == '"')) {
        if (content[pos] == '"') {
            pos++;
            break;
        }
        pos++;
    }
    
    // Find closing quote
    size_t end_pos = pos;
    while (end_pos < content.length() && content[end_pos] != '"') {
        end_pos++;
    }
    
    if (end_pos == content.length()) {
        throw std::runtime_error("Unterminated string value for key: " + key);
    }
    
    return content.substr(pos, end_pos - pos);
}

inline Waypoint MapConfig::get_waypoint(const std::string& name) const {
    for (const auto& wp : _waypoints) {
        if (wp.name == name) {
            return wp;
        }
    }
    throw std::runtime_error("Waypoint not found: " + name);
}

inline Route MapConfig::get_route(const std::string& name) const {
    for (const auto& route : _routes) {
        if (route.name == name) {
            return route;
        }
    }
    throw std::runtime_error("Route not found: " + name);
}

inline std::string MapConfig::get_trajectory_file_path(const std::string& entity_type, unsigned int entity_id) const {
    return _logging.trajectory_dir + "/" + entity_type + "_" + std::to_string(entity_id) + "_trajectory.csv";
}

#endif // MAP_CONFIG_H