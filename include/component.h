#ifndef COMPONENT_H
#define COMPONENT_H

#include <atomic>
#include <string>
#include <thread>
#include <fstream>
#include "vehicle.h"

class Component {
public:
    Component(Vehicle* vehicle, const std::string& name);
    virtual ~Component();

    virtual void start() = 0;
    virtual void stop();
    virtual void join();
    
    bool running() const { return _running; }
    const std::string& name() const { return _name; }
    
protected:
    Vehicle* _vehicle;
    std::string _name;
    std::atomic<bool> _running;
    std::thread _thread;
    
    // CSV logging functionality
    std::ofstream _log_file;
    void open_log_file(const std::string& filename);
    void close_log_file();
};

// Component Implementation
Component::Component(Vehicle* vehicle, const std::string& name) 
    : _vehicle(vehicle), _name(name), _running(false) {
}

Component::~Component() {
    stop();
    join();
    close_log_file();
}

void Component::stop() {
    _running = false;
}

void Component::join() {
    if (_thread.joinable()) {
        _thread.join();
    }
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