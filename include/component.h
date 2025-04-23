#ifndef COMPONENT_H
#define COMPONENT_H

#include <atomic>
#include <string>
#include <pthread.h>
#include <fstream>

#include "traits.h"
#include "communicator.h"

// Forward declaration
class Vehicle;

class Component {
    public:
        template <typename Protocol>
        Component(Vehicle* vehicle, const std::string& name, Protocol* protocol, typename Protocol::Address address);
        
        virtual ~Component();

        virtual void start() = 0;
        virtual void stop();
        
        bool running() const { return _running.load(std::memory_order_acquire); }
        const std::string& name() const { return _name; }
        
        Vehicle* vehicle() const { return _vehicle; };
        std::ofstream* log_file() { return &_log_file; };

        // Send and receive methods that delegate to the communicator
        template <typename Protocol>
        int send(const void* data, unsigned int size);
        
        template <typename Protocol>
        int receive(void* data, unsigned int size);

    protected:
        Vehicle* _vehicle;
        std::string _name;
        std::atomic<bool> _running;
        pthread_t _thread;
        
        // Communicator
        void* _communicator; // Use void* to handle any protocol type
        bool _has_communicator;
        
        // CSV logging functionality
        std::ofstream _log_file;
        void open_log_file(const std::string& filename);
        void close_log_file();
};

// Component Implementation
template <typename Protocol>
Component::Component(Vehicle* vehicle, const std::string& name, Protocol* protocol, typename Protocol::Address address)
    : _vehicle(vehicle), _name(name), _running(false), _has_communicator(false) {
    
    if (protocol) {
        _communicator = new Communicator<Protocol>(protocol, address);
        _has_communicator = true;
    } else {
        _communicator = nullptr;
    }
}

Component::~Component() {
    close_log_file();
    
    if (_has_communicator && _communicator) {
        // Need to cast back to the proper type to delete
        // This is a simplification - in a real implementation you might want to 
        // use type erasure or another technique to properly manage different protocol types
        // Or keep track of which protocol type was used to create the communicator
    }
}

void Component::stop() {
    db<Component>(TRC) << "Component::stop() called for component " << _name << "\n";

    _running.store(false, std::memory_order_release);
    pthread_join(_thread, nullptr);
}

template <typename Protocol>
int Component::send(const void* data, unsigned int size) {
    if (!_has_communicator || !_communicator) {
        return 0;
    }
    
    auto* comm = static_cast<Communicator<Protocol>*>(_communicator);
    Message<Communicator<Protocol>::MAX_MESSAGE_SIZE> msg(data, size);
    return comm->send(&msg) ? 1 : 0;
}

template <typename Protocol>
int Component::receive(void* data, unsigned int size) {
    if (!_has_communicator || !_communicator) {
        return 0;
    }
    
    auto* comm = static_cast<Communicator<Protocol>*>(_communicator);
    Message<Communicator<Protocol>::MAX_MESSAGE_SIZE> msg;
    
    if (!comm->receive(&msg)) {
        return 0;
    }
    
    if (msg.size() > size) {
        return 0;
    }
    
    std::memcpy(data, msg.data(), msg.size());
    return msg.size();
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