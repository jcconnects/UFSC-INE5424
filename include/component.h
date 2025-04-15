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
        
        // Two-phase stop approach
        // Signal component to stop (non-blocking)
        virtual void signal_stop();
        
        // Join the component thread (blocking)
        virtual void join();
        
        // Legacy stop method - now calls signal_stop() followed by join()
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

void Component::signal_stop() {
    db<Component>(TRC) << "[Component " << _name << " on Vehicle " << _vehicle->id() << "] Setting running flag to false.\n";
    _running.store(false, std::memory_order_release);
}

void Component::join() {
    db<Component>(TRC) << "[Component " << _name << " on Vehicle " << _vehicle->id() << "] Joining thread...\n";
    if (_thread != 0) { // Check if thread was ever started
        int join_ret = pthread_join(_thread, nullptr);
        if (join_ret == 0) {
            db<Component>(INF) << "[Component " << _name << " on Vehicle " << _vehicle->id() << "] Thread successfully joined.\n";
        } else {
            // Log error - this indicates a deeper problem if join fails
            db<Component>(ERR) << "[Component " << _name << " on Vehicle " << _vehicle->id() << "] Error joining thread! errno: "
                           << join_ret << " (" << strerror(join_ret) << ")\n";
        }
        _thread = 0; // Invalidate handle
    } else {
        db<Component>(WRN) << "[Component " << _name << " on Vehicle " << _vehicle->id() << "] Join called but thread handle was invalid (never started?).\n";
    }
}

void Component::stop() {
    db<Component>(TRC) << "Component::stop() called for component " << _name << "\n";

    // Call the new two-phase stop methods
    signal_stop();
    join();
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