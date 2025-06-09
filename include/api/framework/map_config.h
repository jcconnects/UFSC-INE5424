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
    double x;
    double y;
};

struct Route {
    std::string name;
    std::vector<std::string> waypoint_names;
};

struct RSUConfig {
    unsigned int id;
    double x;
    double y;
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
    std::string trajectory_generator_script;
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
    const RSUConfig& rsu_config() const { 
        if (_rsu_configs.empty()) {
            return _single_rsu_config;
        }
        return _rsu_configs[0]; 
    }
    std::vector<RSUConfig> get_all_rsu_configs() const { 
        if (_rsu_configs.empty()) {
            return std::vector<RSUConfig>{_single_rsu_config};
        }
        return _rsu_configs; 
    }
    const VehicleConfig& vehicle_config() const { return _vehicle_config; }
    const SimulationConfig& simulation() const { return _simulation; }
    const LoggingConfig& logging() const { return _logging; }
    
    // Helper methods
    Waypoint get_waypoint(const std::string& name) const;
    Route get_route(const std::string& name) const;
    std::string get_trajectory_file_path(const std::string& entity_type, unsigned int entity_id) const;
    double get_transmission_radius() const { return _simulation.default_transmission_radius_m; }
    std::string get_trajectory_generator_script() const { return _simulation.trajectory_generator_script; }

private:
    void parse_config_file(const std::string& file_path);
    std::string extract_json_section(const std::string& content, const std::string& section_name) const;
    double extract_double_value(const std::string& content, const std::string& key) const;
    unsigned int extract_uint_value(const std::string& content, const std::string& key) const;
    std::string extract_string_value(const std::string& content, const std::string& key) const;
    void parse_waypoints(const std::string& content);
    void parse_routes(const std::string& content);
    void parse_single_rsu(const std::string& content);
    void parse_multiple_rsus(const std::string& content);
    
    std::vector<Waypoint> _waypoints;
    std::vector<Route> _routes;
    RSUConfig _single_rsu_config{};  // For single RSU configs (map_1) - zero-initialized
    std::vector<RSUConfig> _rsu_configs;  // For multiple RSU configs (map_2rsu)
    VehicleConfig _vehicle_config{};
    SimulationConfig _simulation{};
    LoggingConfig _logging{};
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
    
    // Try to parse RSU configurations - first try multiple RSUs, then single RSU
    if (content.find("\"rsus\":") != std::string::npos) {
        // Multiple RSUs configuration (array)
        parse_multiple_rsus(content);
    } else if (content.find("\"rsu\":") != std::string::npos) {
        // Single RSU configuration (object)
        try {
            std::string rsu_section = extract_json_section(content, "rsu");
            parse_single_rsu(rsu_section);
        } catch (const std::exception& e) {
            throw std::runtime_error("Error parsing single RSU section: " + std::string(e.what()));
        }
    } else {
        throw std::runtime_error("Neither 'rsu' nor 'rsus' section found in config file");
    }
    
    // Parse vehicle config section
    std::string vehicle_section = extract_json_section(content, "vehicles");
    _vehicle_config.default_count = extract_uint_value(vehicle_section, "default_count");
    _vehicle_config.speed_kmh = extract_double_value(vehicle_section, "speed_kmh");
    
    // Parse simulation config section
    std::string simulation_section = extract_json_section(content, "simulation");
    _simulation.duration_s = extract_uint_value(simulation_section, "duration_s");
    _simulation.update_interval_ms = extract_uint_value(simulation_section, "update_interval_ms");
    _simulation.default_transmission_radius_m = extract_double_value(simulation_section, "default_transmission_radius_m");
    
    // trajectory_generator_script is optional, provide default if not found
    try {
        _simulation.trajectory_generator_script = extract_string_value(simulation_section, "trajectory_generator_script");
    } catch (const std::exception&) {
        _simulation.trajectory_generator_script = "scripts/trajectory_generator_map_1.py"; // Default fallback
    }
    
    // Parse logging config section
    std::string logging_section = extract_json_section(content, "logging");
    _logging.trajectory_dir = extract_string_value(logging_section, "trajectory_dir");
    
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
        wp.x = extract_double_value(waypoint_obj, "x");
        wp.y = extract_double_value(waypoint_obj, "y");
        
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

inline void MapConfig::parse_single_rsu(const std::string& rsu_section) {
    _single_rsu_config.id = extract_uint_value(rsu_section, "id");
    _single_rsu_config.unit = extract_uint_value(rsu_section, "unit");
    _single_rsu_config.broadcast_period = std::chrono::milliseconds(extract_uint_value(rsu_section, "broadcast_period_ms"));
    
    // Parse RSU position subsection
    std::string rsu_position_section = extract_json_section(rsu_section, "position");
    _single_rsu_config.x = extract_double_value(rsu_position_section, "x");
    _single_rsu_config.y = extract_double_value(rsu_position_section, "y");
}

inline void MapConfig::parse_multiple_rsus(const std::string& content) {
    // Simple parser for rsus array
    size_t rsus_start = content.find("\"rsus\":");
    if (rsus_start == std::string::npos) return;
    
    size_t array_start = content.find("[", rsus_start);
    size_t array_end = content.find("]", array_start);
    
    if (array_start == std::string::npos || array_end == std::string::npos) return;
    
    std::string rsus_section = content.substr(array_start + 1, array_end - array_start - 1);
    
    // Parse each RSU object
    size_t pos = 0;
    while (pos < rsus_section.length()) {
        size_t obj_start = rsus_section.find("{", pos);
        if (obj_start == std::string::npos) break;
        
        // Find matching closing brace using proper brace counting
        size_t obj_end = obj_start;
        int brace_count = 0;
        while (obj_end < rsus_section.length()) {
            if (rsus_section[obj_end] == '{') {
                brace_count++;
            } else if (rsus_section[obj_end] == '}') {
                brace_count--;
                if (brace_count == 0) {
                    break;
                }
            }
            obj_end++;
        }
        
        if (brace_count != 0 || obj_end >= rsus_section.length()) {
            break; // Malformed JSON
        }
        
        std::string rsu_obj = rsus_section.substr(obj_start, obj_end - obj_start + 1);
        
        RSUConfig rsu_config;
        rsu_config.id = extract_uint_value(rsu_obj, "id");
        rsu_config.unit = extract_uint_value(rsu_obj, "unit");
        rsu_config.broadcast_period = std::chrono::milliseconds(extract_uint_value(rsu_obj, "broadcast_period_ms"));
        
        // Parse RSU position subsection
        std::string rsu_position_section = extract_json_section(rsu_obj, "position");
        rsu_config.x = extract_double_value(rsu_position_section, "x");
        rsu_config.y = extract_double_value(rsu_position_section, "y");
        
        _rsu_configs.push_back(rsu_config);
        pos = obj_end + 1;
    }
}

inline std::string MapConfig::extract_json_section(const std::string& content, const std::string& section_name) const {
    std::string search_pattern = "\"" + section_name + "\":";
    size_t pos = content.find(search_pattern);
    if (pos == std::string::npos) {
        throw std::runtime_error("Section not found in config: " + section_name);
    }
    
    pos += search_pattern.length();
    
    // Skip whitespace and find opening brace
    while (pos < content.length() && std::isspace(content[pos])) {
        pos++;
    }
    
    if (pos >= content.length() || content[pos] != '{') {
        throw std::runtime_error("Invalid JSON structure for section: " + section_name);
    }
    
    // Find matching closing brace
    size_t start_pos = pos;
    int brace_count = 0;
    while (pos < content.length()) {
        if (content[pos] == '{') {
            brace_count++;
        } else if (content[pos] == '}') {
            brace_count--;
            if (brace_count == 0) {
                break;
            }
        }
        pos++;
    }
    
    if (brace_count != 0) {
        throw std::runtime_error("Unmatched braces in section: " + section_name);
    }
    
    return content.substr(start_pos, pos - start_pos + 1);
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