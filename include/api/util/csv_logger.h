#ifndef CSV_LOGGER_H
#define CSV_LOGGER_H

#include <fstream>
#include <string>
#include <memory>
#include <pthread.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <sys/stat.h>
#include <sys/types.h>

class CSVLogger {
public:
    CSVLogger(const std::string& filepath, const std::string& header);
    ~CSVLogger();
    
    void log(const std::string& csv_line);
    void flush();
    bool is_open() const;
    
    // Static method to create vehicle-specific log directory
    static std::string create_vehicle_log_dir(unsigned int vehicle_id);
    
private:
    std::unique_ptr<std::ofstream> _file;
    pthread_mutex_t _mutex;
    bool _is_open;
    
    std::string get_timestamp();
    static bool create_directory(const std::string& path);
    static std::string get_directory_from_path(const std::string& filepath);
};

// Implementation
CSVLogger::CSVLogger(const std::string& filepath, const std::string& header) : _is_open(false) {
    pthread_mutex_init(&_mutex, nullptr);
    
    // Create directory if it doesn't exist
    std::string dir_path = get_directory_from_path(filepath);
    
    try {
        if (!dir_path.empty()) {
            create_directory(dir_path);
        }
        
        _file = std::make_unique<std::ofstream>(filepath, std::ios::out | std::ios::app);
        if (_file->is_open()) {
            // Check if file is empty (new file) to write header
            _file->seekp(0, std::ios::end);
            if (_file->tellp() == 0) {
                (*_file) << header << std::endl;
            }
            _is_open = true;
        }
    } catch (const std::exception& e) {
        _is_open = false;
    }
}

CSVLogger::~CSVLogger() {
    if (_file && _file->is_open()) {
        _file->close();
    }
    pthread_mutex_destroy(&_mutex);
}

void CSVLogger::log(const std::string& csv_line) {
    if (!_is_open || !_file) return;
    
    pthread_mutex_lock(&_mutex);
    (*_file) << csv_line << std::endl;
    _file->flush();
    pthread_mutex_unlock(&_mutex);
}

void CSVLogger::flush() {
    if (!_is_open || !_file) return;
    
    pthread_mutex_lock(&_mutex);
    _file->flush();
    pthread_mutex_unlock(&_mutex);
}

bool CSVLogger::is_open() const {
    return _is_open;
}

std::string CSVLogger::create_vehicle_log_dir(unsigned int vehicle_id) {
    std::string base_dir = "tests/logs/vehicle_" + std::to_string(vehicle_id);
    
    if (create_directory(base_dir)) {
        return base_dir;
    }
    
    // Fallback to tests/logs
    if (create_directory("tests/logs")) {
        return "tests/logs";
    }
    
    return "."; // Last resort - current directory
}

bool CSVLogger::create_directory(const std::string& path) {
    // Create directory recursively
    std::string current_path;
    size_t pos = 0;
    
    while (pos < path.length()) {
        size_t next_pos = path.find('/', pos);
        if (next_pos == std::string::npos) {
            next_pos = path.length();
        }
        
        current_path += path.substr(pos, next_pos - pos);
        
        if (!current_path.empty()) {
            struct stat st;
            if (stat(current_path.c_str(), &st) != 0) {
                if (mkdir(current_path.c_str(), 0755) != 0) {
                    return false;
                }
            }
        }
        
        if (next_pos < path.length()) {
            current_path += "/";
        }
        pos = next_pos + 1;
    }
    
    return true;
}

std::string CSVLogger::get_directory_from_path(const std::string& filepath) {
    size_t last_slash = filepath.find_last_of('/');
    if (last_slash != std::string::npos) {
        return filepath.substr(0, last_slash);
    }
    return "";
}

std::string CSVLogger::get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

#endif // CSV_LOGGER_H 