#ifndef SHAREDMEMORYENGINE_H
#define SHAREDMEMORYENGINE_H

#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/eventfd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <iostream>
#include <functional>
#include <pthread.h>
#include <sys/epoll.h>
#include <atomic>
#include <deque>
#include <map>

#include "ethernet.h"
#include "traits.h"
#include "debug.h"

// Forward declaration
template <typename T>
class Buffer;

class SharedMemoryEngine {
public:
    // Constants
    static const unsigned int MAX_FRAME_SIZE = 1518; // Max Ethernet frame size
    static const unsigned int MAX_COMPONENTS = 256;  // Maximum number of components
    static const unsigned int MAX_QUEUED_FRAMES = 128; // Maximum frames in queue

    // Component identifier type
    typedef unsigned int ComponentId;
    
    // Frame structure
    struct SharedFrame {
        ComponentId src;
        ComponentId dst;
        unsigned int size;
        unsigned int protocol;
        unsigned char data[MAX_FRAME_SIZE];
    };

public:
    SharedMemoryEngine(ComponentId id);
    
    virtual ~SharedMemoryEngine();

    const bool running();
    
    int send(Ethernet::Frame* frame, unsigned int size);

    static void* run(void* arg);

    void stop();

    ComponentId getId() const { return _component_id; }
    
    // Add accessor for MAC address
    Ethernet::Address getMacAddress() const { return _mac_address; }

protected:
    // Move readFrame from private to protected
    bool readFrame(SharedFrame& frame);
    
    // Signal handler
    virtual void handleSignal() = 0;
    
    ComponentId _component_id;
    Ethernet::Address _mac_address;
    
private:
    // Set up memory and notification primitives
    void setupSharedMemory();
    void setupEpoll();
    
    // Helper method to add a frame to a component's queue
    static void addFrameToQueue(ComponentId component_id, const SharedFrame& frame);
    
private:
    // File descriptors and thread management
    const int _notify_fd;
    int _ep_fd;
    pthread_t _receive_thread;
    bool _running;
    
    // Static shared memory state
    static std::atomic<bool> _memory_initialized;
    static std::map<SharedMemoryEngine::ComponentId, int> _notify_fds;
    static pthread_mutex_t _global_mutex;
    
    // Per-component message queues
    static std::map<ComponentId, std::deque<SharedFrame>> _component_queues;
    static pthread_mutex_t _queue_mutex;
};

/********** SharedMemoryEngine Implementation **********/

// Initialize static members
std::atomic<bool> SharedMemoryEngine::_memory_initialized(false);
std::map<SharedMemoryEngine::ComponentId, int> SharedMemoryEngine::_notify_fds;
pthread_mutex_t SharedMemoryEngine::_global_mutex = PTHREAD_MUTEX_INITIALIZER;
std::map<SharedMemoryEngine::ComponentId, std::deque<SharedMemoryEngine::SharedFrame>> SharedMemoryEngine::_component_queues;
pthread_mutex_t SharedMemoryEngine::_queue_mutex = PTHREAD_MUTEX_INITIALIZER;

SharedMemoryEngine::SharedMemoryEngine(ComponentId id) 
    : _component_id(id), 
      _notify_fd(eventfd(0, EFD_NONBLOCK)),
      _running(false) {
    
    db<SharedMemoryEngine>(TRC) << "SharedMemoryEngine::SharedMemoryEngine() called!\n";
    
    // Generate a MAC-like address for this component
    memset(_mac_address.bytes, 0, Ethernet::MAC_SIZE);
    _mac_address.bytes[0] = 0x02; // Local bit set to indicate local administered address
    _mac_address.bytes[1] = 0x00;
    _mac_address.bytes[2] = 0x00;
    _mac_address.bytes[3] = 0x00;
    _mac_address.bytes[4] = (_component_id >> 8) & 0xFF;
    _mac_address.bytes[5] = _component_id & 0xFF;
    
    db<SharedMemoryEngine>(INF) << "[SharedMemoryEngine] Local address set to: " 
                              << Ethernet::mac_to_string(_mac_address) << "\n";
    
    // Register component globally
    pthread_mutex_lock(&_global_mutex);
    _notify_fds[_component_id] = _notify_fd;
    pthread_mutex_unlock(&_global_mutex);
    
    // Initialize component's queue
    pthread_mutex_lock(&_queue_mutex);
    _component_queues[_component_id] = std::deque<SharedFrame>();
    pthread_mutex_unlock(&_queue_mutex);
    
    setupSharedMemory();
    setupEpoll();
    
    _running = true;
    pthread_create(&_receive_thread, nullptr, SharedMemoryEngine::run, this);
    db<SharedMemoryEngine>(INF) << "[SharedMemoryEngine] receive thread started\n";
}

void SharedMemoryEngine::setupSharedMemory() {
    db<SharedMemoryEngine>(TRC) << "SharedMemoryEngine::setupSharedMemory() called!\n";
    // Nothing to do here for now, since we're using eventfd for notifications
    // and a local queue for frames. In a real implementation, this would
    // set up the shared memory region for inter-process communication.
}

void SharedMemoryEngine::setupEpoll() {
    db<SharedMemoryEngine>(TRC) << "SharedMemoryEngine::setupEpoll() called!\n";

    // 1. Creating epoll
    _ep_fd = epoll_create1(0);
    if (_ep_fd < 0) {
        perror("epoll_create1");
        throw std::runtime_error("Failed to create SharedMemoryEngine::_ep_fd!");
    }

    // 2. Binding notification event on epoll
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = _notify_fd;

    if (epoll_ctl(_ep_fd, EPOLL_CTL_ADD, _notify_fd, &ev) < 0) {
        perror("epoll_ctl");
        throw std::runtime_error("Failed to bind SharedMemoryEngine::_notify_fd to epoll!");
    }

    db<SharedMemoryEngine>(INF) << "[SharedMemoryEngine] epoll setup complete\n";
}

SharedMemoryEngine::~SharedMemoryEngine() {
    db<SharedMemoryEngine>(TRC) << "SharedMemoryEngine::~SharedMemoryEngine() called!\n";
    
    // Remove this component from global registry
    pthread_mutex_lock(&_global_mutex);
    _notify_fds.erase(_component_id);
    pthread_mutex_unlock(&_global_mutex);
    
    // Remove component's queue
    pthread_mutex_lock(&_queue_mutex);
    _component_queues.erase(_component_id);
    pthread_mutex_unlock(&_queue_mutex);
    
    // Clean up resources
    close(_notify_fd);
    close(_ep_fd);
}

const bool SharedMemoryEngine::running() {
    return _running;
}

void SharedMemoryEngine::addFrameToQueue(ComponentId component_id, const SharedFrame& frame) {
    pthread_mutex_lock(&_queue_mutex);
    
    auto it = _component_queues.find(component_id);
    if (it != _component_queues.end()) {
        // Check that the queue isn't too large
        if (it->second.size() < MAX_QUEUED_FRAMES) {
            it->second.push_back(frame);
            db<SharedMemoryEngine>(INF) << "[SharedMemoryEngine] Frame added to component " 
                                     << component_id << " queue\n";
        } else {
            db<SharedMemoryEngine>(ERR) << "[SharedMemoryEngine] Component " 
                                     << component_id << " queue is full\n";
        }
    } else {
        db<SharedMemoryEngine>(ERR) << "[SharedMemoryEngine] Component " 
                                 << component_id << " queue not found\n";
    }
    
    pthread_mutex_unlock(&_queue_mutex);
}

int SharedMemoryEngine::send(Ethernet::Frame* frame, unsigned int size) {
    db<SharedMemoryEngine>(TRC) << "SharedMemoryEngine::send() called!\n";
    
    if (size > MAX_FRAME_SIZE) {
        db<SharedMemoryEngine>(ERR) << "[SharedMemoryEngine] Frame too large: " << size << " bytes\n";
        return -1;
    }
    
    // Make sure the source address is set correctly to maintain consistency
    // between component ID and frame source address
    frame->src = _mac_address;
    
    // Create shared frame
    SharedFrame shared_frame;
    shared_frame.src = _component_id;
    
    // Extract the destination component ID from the MAC address
    // Using last two bytes of the MAC address as component ID
    ComponentId dst_id = (frame->dst.bytes[4] << 8) | frame->dst.bytes[5];
    shared_frame.dst = dst_id;
    
    shared_frame.size = size;
    shared_frame.protocol = frame->prot;
    
    // Copy the frame data into the shared frame
    memcpy(shared_frame.data, frame, size);
    
    // If destination is broadcast, send to all components
    if (memcmp(frame->dst.bytes, "\xff\xff\xff\xff\xff\xff", 6) == 0) {
        db<SharedMemoryEngine>(INF) << "[SharedMemoryEngine] Broadcasting frame\n";
        
        pthread_mutex_lock(&_global_mutex);
        for (const auto& pair : _notify_fds) {
            if (pair.first != _component_id) { // Don't send to self
                // Add frame to the destination's queue
                addFrameToQueue(pair.first, shared_frame);
                
                // Notify the destination component
                uint64_t value = 1;
                write(pair.second, &value, sizeof(value));
            }
        }
        pthread_mutex_unlock(&_global_mutex);
    } 
    // Otherwise send to specific component
    else {
        db<SharedMemoryEngine>(INF) << "[SharedMemoryEngine] Sending frame to component " << dst_id << "\n";
        
        pthread_mutex_lock(&_global_mutex);
        auto it = _notify_fds.find(dst_id);
        if (it != _notify_fds.end()) {
            // Add frame to the destination's queue
            addFrameToQueue(dst_id, shared_frame);
            
            // Notify the destination component
            uint64_t value = 1;
            write(it->second, &value, sizeof(value));
        } else {
            db<SharedMemoryEngine>(ERR) << "[SharedMemoryEngine] Destination component " << dst_id << " not found\n";
            pthread_mutex_unlock(&_global_mutex);
            return -1;
        }
        pthread_mutex_unlock(&_global_mutex);
    }
    
    db<SharedMemoryEngine>(INF) << "[SharedMemoryEngine] Frame sent: {src = " 
                              << Ethernet::mac_to_string(frame->src) 
                              << ", dst = " << Ethernet::mac_to_string(frame->dst) 
                              << ", prot = " << frame->prot << "}\n";
    
    return size;
}

bool SharedMemoryEngine::readFrame(SharedFrame& frame) {
    pthread_mutex_lock(&_queue_mutex);
    
    auto it = _component_queues.find(_component_id);
    if (it == _component_queues.end() || it->second.empty()) {
        pthread_mutex_unlock(&_queue_mutex);
        return false;
    }
    
    frame = it->second.front();
    it->second.pop_front();
    
    pthread_mutex_unlock(&_queue_mutex);
    return true;
}

void* SharedMemoryEngine::run(void* arg) {
    db<SharedMemoryEngine>(TRC) << "SharedMemoryEngine::run() called!\n";

    SharedMemoryEngine* engine = static_cast<SharedMemoryEngine*>(arg);

    struct epoll_event events[10];

    while (engine->running()) {
        int n = epoll_wait(engine->_ep_fd, events, 10, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;

            if (fd == engine->_notify_fd) {
                db<SharedMemoryEngine>(INF) << "[SharedMemoryEngine] Event notification received\n";
                
                // Clear the eventfd
                uint64_t value;
                read(fd, &value, sizeof(value));
                
                // Process frames destined for this component
                engine->handleSignal();
            }
        }
    }

    db<SharedMemoryEngine>(INF) << "[SharedMemoryEngine] receive thread terminated!\n";
    return nullptr;
}

void SharedMemoryEngine::stop() {
    db<SharedMemoryEngine>(TRC) << "SharedMemoryEngine::stop() called!\n";
    
    if (!_running) return;

    _running = false;

    // Send stop signal
    uint64_t value = 1;
    write(_notify_fd, &value, sizeof(value));

    pthread_join(_receive_thread, nullptr);
    db<SharedMemoryEngine>(INF) << "[SharedMemoryEngine] successfully stopped!\n";
}

#endif // SHAREDMEMORYENGINE_H 