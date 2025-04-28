#ifndef NIC_H
#define NIC_H

#include <semaphore.h>
#include <pthread.h>
#include <queue>
#include <atomic>
#include <stdexcept> // For std::runtime_error
#include <sys/epoll.h> // For epoll
#include <sys/eventfd.h> // For eventfd
#include <unistd.h> // For close()

#include "observer.h"
#include "observed.h"
#include "ethernet.h"
#include "buffer.h"
// Include engine implementations directly
#include "socketEngine.h"
#include "sharedMemoryEngine.h"
#include "traits.h"
#include "debug.h"

// Foward Declaration
class Initializer;

// Network Interface Card implementation
// Templated on the External (Network) and Internal (Local) communication engines
template <typename ExternalEngine, typename InternalEngine>
class NIC: public Ethernet, public Conditionally_Data_Observed<Buffer<Ethernet::Frame>, Ethernet::Protocol>
{
    friend class Initializer;

    public:
        // Combine buffer counts from traits (assuming traits might specify per-engine needs)
        // TODO: Revisit Trait definition for dual engines if needed
        static const unsigned int N_BUFFERS = Traits<NIC<ExternalEngine, InternalEngine>>::SEND_BUFFERS + Traits<NIC<ExternalEngine, InternalEngine>>::RECEIVE_BUFFERS;

        typedef Ethernet::Address Address;
        typedef Ethernet::Protocol Protocol_Number;
        typedef Buffer<Ethernet::Frame> DataBuffer;
        typedef Conditional_Data_Observer<DataBuffer, Protocol_Number> Observer;
        typedef Conditionally_Data_Observed<DataBuffer, Protocol_Number> Observed;
        
        // Statistics for network operations (can be expanded later for internal/external)
        struct Statistics {
            std::atomic<unsigned int> packets_sent_external;
            std::atomic<unsigned int> packets_received_external;
            std::atomic<unsigned int> bytes_sent_external;
            std::atomic<unsigned int> bytes_received_external;
            std::atomic<unsigned int> tx_drops_external;
            std::atomic<unsigned int> rx_drops_external;

            std::atomic<unsigned int> packets_sent_internal;
            std::atomic<unsigned int> packets_received_internal;
            std::atomic<unsigned int> bytes_sent_internal;
            std::atomic<unsigned int> bytes_received_internal;
            std::atomic<unsigned int> tx_drops_internal;
            std::atomic<unsigned int> rx_drops_internal;
            
            Statistics() : 
                packets_sent_external(0), packets_received_external(0),
                bytes_sent_external(0), bytes_received_external(0),
                tx_drops_external(0), rx_drops_external(0),
                packets_sent_internal(0), packets_received_internal(0),
                bytes_sent_internal(0), bytes_received_internal(0),
                tx_drops_internal(0), rx_drops_internal(0) {}
        };

    protected:
        // Constructor is protected if only Initializer should create it
        NIC();

    public:
        ~NIC();
        
        // Send a pre-allocated buffer (routes automatically)
        int send(DataBuffer* buf);

        // Process a received buffer (called by Protocol layer)
        // This method essentially unwraps the payload from the DataBuffer
        int receive(DataBuffer* buf, Address* src, Address* dst, void* data, unsigned int size);
        
        // Allocate a buffer for sending
        DataBuffer* alloc(Address dst, Protocol_Number prot, unsigned int size);
        
        // Free a buffer after use
        void free(DataBuffer* buf);
        
        // Get the NIC's primary (external) address
        const Address& address();
        
        // Get network statistics
        const Statistics& statistics();
        
        // Explicitly stop the NIC and its underlying engines
        void stop();
        
        // Attach/detach observers (inherited from Conditionally_Data_Observed)
        // void attach(Observer* obs, Protocol_Number prot);
        // void detach(Observer* obs, Protocol_Number prot);

    private:
        // Event handling for data arrival from engines
        void handleExternalEvent();
        void handleInternalEvent();

        // NIC's own event loop
        static void* eventLoop(void* arg);
        void setupNicEpoll();

    private:
        // Engine instances
        ExternalEngine _external_engine;
        InternalEngine _internal_engine;

        // NIC state and event handling
        Address _address; // Primary MAC address (from ExternalEngine)
        std::atomic<bool> _running;
        int _nic_ep_fd;      // NIC's own epoll FD
        int _stop_event_fd;  // eventfd for stopping the event loop
        pthread_t _event_thread; // Thread for eventLoop

        // Statistics
        Statistics _statistics;

        // Buffer management
        DataBuffer _buffer[N_BUFFERS];
        std::queue<DataBuffer*> _free_buffers;
        unsigned int _free_buffer_count; // Number of free buffers
        sem_t _binary_sem; // Mutex for _free_buffers queue
};

// NIC implementations
template <typename ExternalEngine, typename InternalEngine>
NIC<ExternalEngine, InternalEngine>::NIC()
    : _running(false),
      _nic_ep_fd(-1),
      _stop_event_fd(-1),
      _event_thread(0)
{
    db<NIC>(TRC) << "NIC<ExternalEngine, InternalEngine>::NIC() called!\n";

    try {
        // Initialize buffer pool
    for (unsigned int i = 0; i < N_BUFFERS; ++i) {
        _buffer[i] = DataBuffer();
        _free_buffers.push(&_buffer[i]);
    }
    _free_buffer_count = N_BUFFERS;
    db<NIC>(INF) << "[NIC] " << std::to_string(N_BUFFERS) << " buffers created\n";
    
    sem_init(&_binary_sem, 0, 1);
    
        // Get address from the External Engine
        // Assumes ExternalEngine provides getMacAddress()
        _address = _external_engine.getMacAddress();
        db<NIC>(INF) << "[NIC] Address set from ExternalEngine: " << Ethernet::mac_to_string(_address) << "\n";
    
        // Initialize and setup the NIC's epoll
        _stop_event_fd = eventfd(0, EFD_NONBLOCK);
        if (_stop_event_fd < 0) {
            perror("eventfd");
            throw std::runtime_error("Failed to create NIC stop eventfd");
        }
        db<NIC>(INF) << "[NIC] Stop eventfd created: " << _stop_event_fd << "\n";
        // Engines should be started *before* the event loop if they manage their own threads/resources
        // Assuming engines have a start() method or are ready after construction
        _external_engine.start(); // If needed
        _internal_engine.start(); // If needed

        setupNicEpoll(); // Sets up _nic_ep_fd and adds engine FDs

        // Start the NIC's event loop thread
        _running.store(true, std::memory_order_release);
        int rc = pthread_create(&_event_thread, nullptr, NIC::eventLoop, this);
        if (rc) {
            _running.store(false); // Creation failed, not running
            close(_nic_ep_fd); // Clean up epoll fd
            close(_stop_event_fd); // Clean up eventfd
            sem_destroy(&_binary_sem);
            db<NIC>(ERR) << "ERROR; return code from pthread_create() is " << rc << "\n";
            throw std::runtime_error("Failed to create NIC event loop thread");
        }
        db<NIC>(INF) << "[NIC] Event loop thread started.\n";

    } catch (const std::exception& e) {
        db<NIC>(ERR) << "NIC Constructor failed: " << e.what() << "\n";
        // Ensure partial resources are cleaned up if possible
        if (_nic_ep_fd >= 0) close(_nic_ep_fd);
        if (_stop_event_fd >= 0) close(_stop_event_fd);
        sem_destroy(&_binary_sem);
        // Rethrow or handle appropriately
        throw;
    }
}

template <typename ExternalEngine, typename InternalEngine>
NIC<ExternalEngine, InternalEngine>::~NIC() {
    db<NIC>(TRC) << "NIC<ExternalEngine, InternalEngine>::~NIC() called!\n";

    stop(); // Ensure everything is stopped and joined
    
    sem_destroy(&_binary_sem);

    // Engines (_external_engine, _internal_engine) are destructed automatically
}

template <typename ExternalEngine, typename InternalEngine>
void NIC<ExternalEngine, InternalEngine>::setupNicEpoll() {
    db<NIC>(TRC) << "NIC::setupNicEpoll() called!\n";

    _nic_ep_fd = epoll_create1(0);
    if (_nic_ep_fd < 0) {
        perror("epoll_create1 NIC");
        throw std::runtime_error("Failed to create NIC epoll instance");
    }

    struct epoll_event ev = {};

    // 1. Register stop event FD
    ev.events = EPOLLIN;
    ev.data.fd = _stop_event_fd;
    if (epoll_ctl(_nic_ep_fd, EPOLL_CTL_ADD, _stop_event_fd, &ev) < 0) {
        perror("epoll_ctl add stop_event_fd");
        close(_nic_ep_fd); // Clean up already created epoll fd
        throw std::runtime_error("Failed to add stop_event_fd to NIC epoll");
    }

    // 2. Register ExternalEngine notification FD
    // Assumes ExternalEngine provides getNotificationFd()
    int external_fd = _external_engine.getNotificationFd();
    if (external_fd < 0) {
         throw std::runtime_error("ExternalEngine provided invalid notification FD");
    }
    ev.events = EPOLLIN;
    ev.data.fd = external_fd;
    if (epoll_ctl(_nic_ep_fd, EPOLL_CTL_ADD, external_fd, &ev) < 0) {
        perror("epoll_ctl add external_fd");
        close(_nic_ep_fd);
        throw std::runtime_error("Failed to add ExternalEngine FD to NIC epoll");
    }
     db<NIC>(INF) << "[NIC] ExternalEngine FD (" << external_fd << ") added to epoll.\n";

    db<NIC>(INF) << "[NIC] Epoll setup complete.\n";
}

template <typename ExternalEngine, typename InternalEngine>
void* NIC<ExternalEngine, InternalEngine>::eventLoop(void* arg) {
    NIC* self = static_cast<NIC*>(arg);
    db<NIC>(TRC) << "NIC::eventLoop() thread started.\n";

    const int MAX_EVENTS = 10;
    struct epoll_event events[MAX_EVENTS];

    int external_notify_fd = self->_external_engine.getNotificationFd();
    int internal_notify_fd = self->_internal_engine.getNotificationFd();

    while (self->_running.load(std::memory_order_acquire)) {
        db<NIC>(INF) << "[NIC EventLoop] Waiting for events...\n";
        int n = epoll_wait(self->_nic_ep_fd, events, MAX_EVENTS, -1); // Wait indefinitely

        if (n < 0) {
            if (errno == EINTR) {
                db<NIC>(WRN) << "[NIC EventLoop] epoll_wait interrupted, continuing...\n";
                continue; // Interrupted by signal, safe to continue if _running is true
            }
            perror("epoll_wait NIC");
             db<NIC>(ERR) << "[NIC EventLoop] epoll_wait error (" << errno << "), terminating loop.\n";
            self->_running.store(false); // Ensure loop terminates on error
            break;
        }
        db<NIC>(INF) << "[NIC EventLoop] " << n << " events received.\n";

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;

            if (!self->_running.load(std::memory_order_acquire)) {
                db<NIC>(INF) << "[NIC EventLoop] Detected stop signal during event processing.\n";
                break; // Exit loop if stopped during processing
            }

            if (fd == self->_stop_event_fd) {
                db<NIC>(INF) << "[NIC EventLoop] Stop event received.\n";
                uint64_t signal_value;
                read(self->_stop_event_fd, &signal_value, sizeof(signal_value)); // Clear the eventfd
                self->_running.store(false); // Ensure flag is set
                break; // Exit the event processing loop
            } else if (fd == external_notify_fd) {
                db<NIC>(INF) << "[NIC EventLoop] External engine event detected.\n";
                self->handleExternalEvent();
            } else {
                 db<NIC>(WRN) << "[NIC EventLoop] Unknown FD triggered epoll: " << fd << "\n";
            }
        }
        // Re-check running status after processing all events in this batch
        if (!self->_running.load(std::memory_order_acquire)) {
             db<NIC>(INF) << "[NIC EventLoop] Stop detected after event batch processing.\n";
            break;
        }
    }

    db<NIC>(INF) << "[NIC EventLoop] Thread terminated.\n";
    return nullptr;
}

template <typename ExternalEngine, typename InternalEngine>
int NIC<ExternalEngine, InternalEngine>::send(DataBuffer* buf) {
    db<NIC>(TRC) << "NIC::send() called!\n";

    if (!_running.load(std::memory_order_acquire)) {
        db<NIC>(WRN) << "[NIC] send() called while NIC is not running, dropping packet.\n";
        // Dropping requires freeing the buffer if it was allocated
        // free(buf); // Caller should handle freeing if send fails this way? Or NIC takes ownership?
                       // Let's assume caller handles free if send returns < 0 for now.
        return -1; // Indicate error
    }

    if (!buf || !buf->data()) {
        db<NIC>(ERR) << "[NIC] send() called with null or invalid buffer.\n";
        // statistics.tx_drops++; // Need separate internal/external drops
        return -1;
    }

    const Address& dest_mac = buf->data()->dst;
    const Address& my_mac = _address; // NIC's primary MAC
    int result = -1;

    // Routing Decision: If destination is self, use internal engine. Otherwise, external.
    if (dest_mac == my_mac) {
        db<NIC>(INF) << "[NIC] Routing frame locally via InternalEngine (dst == self)\n";
        // Assumes InternalEngine::send() takes Ethernet::Frame* and size
        result = _internal_engine.send(buf->data(), buf->size());
        if (result > 0) {
            _statistics.packets_sent_internal++;
            _statistics.bytes_sent_internal += result;
            handleInternalEvent(); // Notify internal engine of sent data
        } else {
            _statistics.tx_drops_internal++;
            db<NIC>(WRN) << "[NIC] InternalEngine::send failed (result=" << result << ")\n";
        }
    } else {
        db<NIC>(INF) << "[NIC] Routing frame externally via ExternalEngine (dst != self)\n";
        // Assumes ExternalEngine::send() takes Ethernet::Frame* and size
        result = _external_engine.send(buf->data(), buf->size());
         if (result > 0) {
            _statistics.packets_sent_external++;
            _statistics.bytes_sent_external += result;
        } else {
            _statistics.tx_drops_external++;
            db<NIC>(WRN) << "[NIC] ExternalEngine::send failed (result=" << result << ")\n";
        }
    }

    db<NIC>(INF) << "[NIC] send() returning " << result << "\n";
    return result; // Return bytes sent or negative on error
}


template <typename ExternalEngine, typename InternalEngine>
int NIC<ExternalEngine, InternalEngine>::receive(DataBuffer* buf, Address* src, Address* dst, void* data, unsigned int size) {
    db<NIC>(TRC) << "NIC::receive() called! (Unwrapping buffer for Protocol)\n";

    // This method is now primarily for the Protocol layer to extract data from a buffer
    // received via the notify mechanism.

    if (!buf || !buf->data()) {
        db<NIC>(ERR) << "[NIC] receive() called with null buffer or null buffer data\n";
        // Statistics updated when packet dropped during actual reception handling
        // Should free be called here? If protocol gets null, maybe it handles it?
        // If protocol gets a valid buffer pointer from notify, it owns it until free.
        // If buf is null here, it's an API misuse by Protocol.
        return -1;
    }

    // Add safety check for unreasonable buffer sizes
    static const unsigned int MAX_EXPECTED_FRAME_SIZE = 1518;
    if (buf->size() < Ethernet::HEADER_SIZE || buf->size() > MAX_EXPECTED_FRAME_SIZE) {
        db<NIC>(ERR) << "[NIC] receive() called with invalid buffer size: " << buf->size() << "\n";
        // Protocol layer should free the buffer it received via notify
        return -1;
    }

    if (!data || size == 0) {
        db<NIC>(WRN) << "[NIC] receive() called with null data pointer or zero size.\n";
        // Protocol layer should free the buffer
        return -1;
    }

    Ethernet::Frame* frame = buf->data();
    
    // 1. Fill src and dst addresses if requested
    if (src) *src = frame->src;
    if (dst) *dst = frame->dst;
    
    // 2. Calculate payload size
    unsigned int payload_size = buf->size() - Ethernet::HEADER_SIZE;
    db<NIC>(INF) << "[NIC] Extracted frame from buffer: {src=" << Ethernet::mac_to_string(frame->src)
                 << ", dst=" << Ethernet::mac_to_string(frame->dst) << ", prot=" << frame->prot
                 << ", total_size=" << buf->size() << ", payload_size=" << payload_size << "}\n";
    
    // 3. Check if user buffer is large enough
    if (payload_size > size) {
        db<NIC>(ERR) << "[NIC] User buffer too small (" << size << " bytes) for payload (" << payload_size << " bytes).\n";
        // Protocol layer should free the buffer
        return -2; // Indicate buffer overflow error
    }

    // 4. Copy payload to user's data buffer
    std::memcpy(data, frame->payload, payload_size);

    // 5. Protocol layer is responsible for freeing the buffer via NIC::free()
    // free(buf); // DO NOT free here

    // 6. Return size of copied payload
    return payload_size;
}

template <typename ExternalEngine, typename InternalEngine>
void NIC<ExternalEngine, InternalEngine>::handleExternalEvent() {
    db<NIC>(TRC) << "NIC::handleExternalEvent() called!\n";
    
    // Assume ExternalEngine provides a method like receiveFrame that fills a buffer
    // and returns size, or < 0 on error/no data. Needs precise definition.
    // Example: int receiveFrame(Ethernet::Frame& frame_buffer);

    Ethernet::Frame received_frame_data; // Temporary storage for the frame
    int bytes_received = _external_engine.receiveFrame(received_frame_data); // Hypothetical call

    if (bytes_received <= 0) {
        if (bytes_received < 0) { // Log errors, ignore non-errors (like EAGAIN if it were non-blocking)
             db<NIC>(ERR) << "[NIC] ExternalEngine::receiveFrame failed or returned no data (" << bytes_received << ")\n";
             // Potentially update rx_drops_external if it was a real error
        }
        return; // No data or error
    }

    // Basic validation
    if (static_cast<unsigned int>(bytes_received) < Ethernet::HEADER_SIZE) {
        db<NIC>(ERR) << "[NIC] Received undersized frame externally (" << bytes_received << " bytes)\n";
        _statistics.rx_drops_external++;
        return;
    }
    
    // Frame already contains src, dst, prot (assuming receiveFrame populates these correctly,
    // potentially handling byte order conversion internally).
    // Let's assume protocol is already in host byte order here.
    Ethernet::Address src = received_frame_data.src;
    Ethernet::Address dst = received_frame_data.dst;
    Protocol_Number proto = received_frame_data.prot; // Assume host order

    // Filter check (optional, engine might do this): ignore packets sent by self
    if (src == _address) {
         db<NIC>(INF) << "[NIC External] Ignoring frame from self: {src=" << Ethernet::mac_to_string(src) << "}\n";
         return;
    }
    // Filter check: ignore packets not for us or broadcast
    if (dst != _address && dst != Ethernet::BROADCAST) {
         db<NIC>(INF) << "[NIC External] Ignoring frame not for this NIC: {dst=" << Ethernet::mac_to_string(dst) << "}\n";
        return;
    }


    db<NIC>(INF) << "[NIC External] Received frame: {src=" << Ethernet::mac_to_string(src)
                 << ", dst=" << Ethernet::mac_to_string(dst) << ", prot=" << proto
                 << ", size=" << bytes_received << "}\n";
    
    // Allocate a NIC buffer
    DataBuffer* buf = alloc(dst, proto, bytes_received);
    if (!buf) {
        db<NIC>(ERR) << "[NIC External] Failed to allocate buffer for received frame!\n";
        _statistics.rx_drops_external++;
        return; // Drop packet
    }

    // Copy the entire received frame into the buffer
    std::memcpy(buf->data(), &received_frame_data, bytes_received);
    // Ensure buffer size is set correctly (alloc might set based on header + payload size, double check)
    buf->setSize(bytes_received); // Explicitly set total size

    // Notify observers (typically the Protocol layer)
    db<NIC>(INF) << "[NIC External] Notifying observers for protocol " << proto << "\n";
    if (!Observed::notify(proto, buf)) {
        db<NIC>(INF) << "[NIC External] Data received, but no observer for protocol " << proto << ". Freeing buffer.\n";
        _statistics.rx_drops_external++; // Count as drop if no one is listening
        free(buf); // If no one is listening, free the buffer
    } else {
        // Notification successful, update stats
        _statistics.packets_received_external++;
        _statistics.bytes_received_external += bytes_received;
         db<NIC>(INF) << "[NIC External] Notification successful for protocol " << proto << ". Buffer passed to observer.\n";
    }
}


template <typename ExternalEngine, typename InternalEngine>
void NIC<ExternalEngine, InternalEngine>::handleInternalEvent() {
    db<NIC>(TRC) << "NIC::handleInternalEvent() called!\n";

    // Assume InternalEngine provides a method like receiveData that provides
    // payload, size, and crucial metadata (like original protocol).
    // Example: int receiveData(void* payload_buf, unsigned int max_size, Protocol_Number* proto_out);
    // Need a way to get payload, size, and protocol number.

    unsigned char internal_payload[Traits<InternalEngine>::MTU]; // Assuming InternalEngine has MTU trait
    Protocol_Number received_proto;
    int payload_size = _internal_engine.receiveData(internal_payload, sizeof(internal_payload), &received_proto); // Hypothetical

    if (payload_size <= 0) {
        if (payload_size < 0) {
            db<NIC>(ERR) << "[NIC] InternalEngine::receiveData failed or returned no data (" << payload_size << ")\n";
            // Potentially update rx_drops_internal
        }
        return; // No data or error
    }


    db<NIC>(INF) << "[NIC Internal] Received data: {proto=" << received_proto
                 << ", payload_size=" << payload_size << "}\n";

    // Reconstruct an Ethernet::Frame for the Protocol layer
    unsigned int total_frame_size = Ethernet::HEADER_SIZE + payload_size;

    // Allocate a NIC buffer
    // Destination MAC for internally received frames is the NIC itself
    DataBuffer* buf = alloc(_address, received_proto, total_frame_size);
    if (!buf) {
        db<NIC>(ERR) << "[NIC Internal] Failed to allocate buffer for received data!\n";
        _statistics.rx_drops_internal++;
        return; // Drop packet
    }

    // Fill the Ethernet header in the allocated buffer
    Ethernet::Frame* frame = buf->data();
    frame->dst = _address; // Frame is destined for this NIC (local component)
    frame->src = _address; // Frame originates internally (from this NIC's perspective)
                           // TODO: Can InternalEngine provide original sender component info?
                           // If so, maybe use a local MAC scheme for frame->src?
                           // For now, using _address keeps it simple for Protocol layer.
    frame->prot = received_proto; // Protocol number provided by InternalEngine

    // Copy the payload
    std::memcpy(frame->payload, internal_payload, payload_size);

    // Ensure buffer size is set correctly
    buf->setSize(total_frame_size);

    // Notify observers
    db<NIC>(INF) << "[NIC Internal] Notifying observers for protocol " << received_proto << "\n";
    if (!Observed::notify(received_proto, buf)) {
        db<NIC>(INF) << "[NIC Internal] Data received, but no observer for protocol " << received_proto << ". Freeing buffer.\n";
        _statistics.rx_drops_internal++; // Count as drop
        free(buf);
    } else {
        _statistics.packets_received_internal++;
        _statistics.bytes_received_internal += total_frame_size; // Store total frame size for consistency?
        db<NIC>(INF) << "[NIC Internal] Notification successful for protocol " << received_proto << ". Buffer passed to observer.\n";
    }
}


template <typename ExternalEngine, typename InternalEngine>
typename NIC<ExternalEngine, InternalEngine>::DataBuffer* NIC<ExternalEngine, InternalEngine>::alloc(Address dst, Protocol_Number prot, unsigned int size) {
    db<NIC>(TRC) << "NIC::alloc() called!\n";

    // Check if NIC is running before potentially blocking on semaphore
    if (!_running.load(std::memory_order_acquire)) {
        db<NIC>(WRN) << "[NIC] alloc() called when NIC is not running.\n";
        return nullptr;
    }

    sem_wait(&_binary_sem); // Lock the queue
    // Check if there are free buffers available
    if (!_free_buffer_count) {
        db<NIC>(WRN) << "[NIC] No free buffers available for allocation.\n";
        sem_post(&_binary_sem); // Release semaphore
        return nullptr; // No buffer available
    }
    // TODO - review if this is needed
    // Re-check running status after acquiring semaphore, in case stop() was called
    if (!_running.load(std::memory_order_acquire)) {
        db<NIC>(WRN) << "[NIC] alloc() acquired semaphore but NIC stopped.\n";
        return nullptr;
    }

    DataBuffer* buf = _free_buffers.front();
    _free_buffers.pop();
    _free_buffer_count--; // Decrement available buffer count
    sem_post(&_binary_sem); // Unlock the queue

    // Set frame headers
    buf->data()->src = _address; // Source is always the NIC's address
    buf->data()->dst = dst;
    buf->data()->prot = prot;
    buf->setSize(size); // Set total frame size

    db<NIC>(INF) << "[NIC] buffer allocated for frame: {src=" << Ethernet::mac_to_string(_address)
                 << ", dst=" << Ethernet::mac_to_string(dst) << ", prot=" << prot
                 << ", size=" << size << "}\n";

    return buf;
}

template <typename ExternalEngine, typename InternalEngine>
void NIC<ExternalEngine, InternalEngine>::free(DataBuffer* buf) {
    db<NIC>(TRC) << "NIC::free() called!\n";

    if (!buf) {
         db<NIC>(WRN) << "[NIC] free() called with null buffer.\n";
         return;
    }

    // Optional: Clear buffer data for security/debugging
    buf->clear(); // Assuming this resets internal state/size

    db<NIC>(INF) << "[NIC] buffer released.\n";

    sem_wait(&_binary_sem); // Lock queue
    _free_buffers.push(buf);
    _free_buffer_count++; // Increment available buffer count
    sem_post(&_binary_sem); // Unlock queue
}

template <typename ExternalEngine, typename InternalEngine>
const typename NIC<ExternalEngine, InternalEngine>::Address& NIC<ExternalEngine, InternalEngine>::address() {
    // Returns the primary MAC address associated with the external interface
    db<NIC>(TRC) << "NIC::address() called!\n";
    return _address;
}

template <typename ExternalEngine, typename InternalEngine>
const typename NIC<ExternalEngine, InternalEngine>::Statistics& NIC<ExternalEngine, InternalEngine>::statistics() {
    db<NIC>(TRC) << "NIC::statistics() called!\n";
    return _statistics;
}


template <typename ExternalEngine, typename InternalEngine>
void NIC<ExternalEngine, InternalEngine>::stop() {
    db<NIC>(TRC) << "NIC::stop() called!\n";

    if (!_running.load(std::memory_order_acquire)) {
        db<NIC>(INF) << "[NIC] stop() called but already stopped.\n";
        return;
    }

    _running.store(false, std::memory_order_release); // Signal threads to stop

    // Signal the event loop thread to wake up and exit
    if (_stop_event_fd >= 0) {
        uint64_t signal = 1;
        int bytes_written = write(_stop_event_fd, &signal, sizeof(signal));
        if (bytes_written < 0) {
             perror("write to stop_event_fd");
             db<NIC>(ERR) << "[NIC] Failed to write to stop_event_fd!\n";
        } else {
             db<NIC>(INF) << "[NIC] Stop signal sent to event loop.\n";
        }
    } else {
        db<NIC>(WRN) << "[NIC] Stop event FD invalid during stop().\n";
    }


    // Stop the engines (if they have background tasks/threads)
    // Assumes engines have stop() methods
    db<NIC>(INF) << "[NIC] Stopping External Engine...\n";
    _external_engine.stop();
    db<NIC>(INF) << "[NIC] Stopping Internal Engine...\n";
    _internal_engine.stop();


    // Join the event loop thread
    if (_event_thread != 0) {
        db<NIC>(INF) << "[NIC] Joining event loop thread...\n";
        int join_rc = pthread_join(_event_thread, nullptr);
        if (join_rc == 0) {
             db<NIC>(INF) << "[NIC] Event loop thread joined successfully.\n";
        } else {
             db<NIC>(ERR) << "[NIC] Failed to join event loop thread! Error code: " << join_rc << "\n";
             // Log strerror(join_rc) for more details
        }
         _event_thread = 0; // Reset thread handle
    }

    // Close FDs owned by NIC
    if (_nic_ep_fd >= 0) {
        close(_nic_ep_fd);
        _nic_ep_fd = -1;
    }
     if (_stop_event_fd >= 0) {
        close(_stop_event_fd);
        _stop_event_fd = -1;
    }

    db<NIC>(INF) << "[NIC] Stop sequence complete.\n";
}
#endif // NIC_H