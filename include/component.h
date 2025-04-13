#ifndef COMPONENT_H
#define COMPONENT_H

#include <atomic>
#include <string>
#include <pthread.h>
#include <fstream>
#include <cstring>
#include <time.h>

#include "traits.h"

// Forward declaration
class Vehicle;

class Component {
    public:
        Component(Vehicle* vehicle, const std::string& name);
        virtual ~Component();

        virtual void start() = 0;
        virtual void stop();
        
        bool running() const { return _running; }
        const std::string& name() const { return _name; }
        
        Vehicle* vehicle() const { return _vehicle; };
        std::ofstream* log_file() { return &_log_file; };

    protected:
        Vehicle* _vehicle;
        std::string _name;
        std::atomic<bool> _running;
        pthread_t _thread;
        
        // CSV logging functionality
        std::ofstream _log_file;
        pthread_mutex_t _log_mutex;  // Add mutex for thread-safe CSV file operations
        void open_log_file(const std::string& filename);
        void close_log_file();
        
        // Thread-safe logging method
        void write_to_log(const std::string& line) {
            pthread_mutex_lock(&_log_mutex);
            if (_log_file.is_open()) {
                _log_file << line;
                _log_file.flush();
            }
            pthread_mutex_unlock(&_log_mutex);
        }
};

// Component Implementation
Component::Component(Vehicle* vehicle, const std::string& name)  : _vehicle(vehicle), _name(name), _running(false) {
    // Initialize mutex
    pthread_mutex_init(&_log_mutex, nullptr);
}

Component::~Component() {
    close_log_file();
    pthread_mutex_destroy(&_log_mutex);
}

void Component::stop() {
    db<Component>(TRC) << "Component::stop() called for component " << _name << "\n";

    _running = false;
    
    if (_thread != 0) { // Basic check if thread handle seems valid
        db<Component>(TRC) << "[Component " << _name << " on Vehicle " << _vehicle->id() << "] Attempting to join thread...\n";
        
        // Set a timeout for join (100ms) to avoid infinite waiting
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 0;  // 0 additional seconds
        timeout.tv_nsec += 100000000; // 100ms in nanoseconds
        
        // Handle nanosecond overflow
        if (timeout.tv_nsec >= 1000000000) {
            timeout.tv_sec += 1;
            timeout.tv_nsec -= 1000000000;
        }
        
        // Try timed join first
        int join_ret = pthread_timedjoin_np(_thread, nullptr, &timeout);
        
        if (join_ret == 0) {
            db<Component>(TRC) << "[Component " << _name << " on Vehicle " << _vehicle->id() << "] Thread successfully joined.\n";
        } else if (join_ret == ETIMEDOUT) {
            db<Component>(WRN) << "[Component " << _name << " on Vehicle " << _vehicle->id() 
                            << "] Thread join timed out after 100ms. Thread may be blocked or leaking.\n";
            // We continue without waiting for the thread
        } else {
            db<Component>(ERR) << "[Component " << _name << " on Vehicle " << _vehicle->id() << "] Error joining thread! errno: " 
                            << join_ret << " (" << strerror(join_ret) << ")\n";
        }
        
        _thread = 0; // Invalidate thread handle after join attempt
    } else {
        db<Component>(WRN) << "[Component " << _name << " on Vehicle " << _vehicle->id() << "] Stop called but thread handle was invalid (already stopped or never started?).\n";
    }
}

void Component::open_log_file(const std::string& filename) {
    pthread_mutex_lock(&_log_mutex);
    _log_file.open(filename);
    // Each specific component should implement its own header
    pthread_mutex_unlock(&_log_mutex);
}

void Component::close_log_file() {
    pthread_mutex_lock(&_log_mutex);
    if (_log_file.is_open()) {
        _log_file.close();
    }
    pthread_mutex_unlock(&_log_mutex);
}

#endif // COMPONENT_H 