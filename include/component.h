#ifndef COMPONENT_H
#define COMPONENT_H

#include <atomic>
#include <string>
#include <pthread.h>
#include <fstream>
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
    
    // Try to join with a timeout - up to 3 seconds
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 3;
    
    int join_result = pthread_timedjoin_np(_thread, nullptr, &ts);
    if (join_result == 0) {
        db<Component>(TRC) << "[Component " << _name << "] thread joined successfully\n";
    } else if (join_result == ETIMEDOUT) {
        db<Component>(ERR) << "[Component " << _name << "] thread join timed out, may have deadlocked\n";
        // If the join times out, we just continue - can't do much else
    } else {
        db<Component>(ERR) << "[Component " << _name << "] thread join failed with error: " << join_result << "\n";
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