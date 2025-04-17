#ifndef COMPONENT_H
#define COMPONENT_H

#include <atomic>
#include <string>
#include <pthread.h>
#include <fstream>

#include "traits.h"

// Forward declaration
class Vehicle;

class Component {
    public:
        Component(Vehicle* vehicle, const std::string& name);
        virtual ~Component();

        virtual void start() = 0;
        virtual void stop();
        
        bool running() const { return _running.load(std::memory_order_acquire); }
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
        void open_log_file(const std::string& filename);
        void close_log_file();
};

// Component Implementation
Component::Component(Vehicle* vehicle, const std::string& name)  : _vehicle(vehicle), _name(name), _running(false) {}

Component::~Component() {
    close_log_file();
}

void Component::stop() {
    db<Component>(TRC) << "Component::stop() called for component " << _name << "\n";

    _running.store(false, std::memory_order_release);
    pthread_join(_thread, nullptr);
}

void Component::open_log_file(const std::string& filename) {
    _log_file.open(filename);
    // Each specific component should implement its own header
}

void Component::close_log_file() {
    if (_log_file.is_open()) {
        _log_file.close();
    }
}

#endif // COMPONENT_H 