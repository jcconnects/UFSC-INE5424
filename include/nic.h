#ifndef NIC_H
#define NIC_H

#include <semaphore.h>
#include <pthread.h>
#include <queue>
#include <atomic>

#include "observer.h"
#include "observed.h"
#include "ethernet.h"
#include "buffer.h"
#include "socketEngine.h"
#include "traits.h"
#include "debug.h"

// Foward Declaration
class Initializer;

// Network Interface Card implementation
template <typename Engine>
class NIC: public Ethernet, public Conditionally_Data_Observed<Buffer<Ethernet::Frame>, Ethernet::Protocol>, private Engine
{
    friend class Initializer;

    public:
        static const unsigned int BUFFER_SIZE = Traits<NIC<Engine>>::SEND_BUFFERS * sizeof(Buffer<Ethernet::Frame>) + Traits<NIC<Engine>>::RECEIVE_BUFFERS * sizeof(Buffer<Ethernet::Frame>);
        static const unsigned int N_BUFFERS = Traits<NIC<Engine>>::SEND_BUFFERS + Traits<NIC<Engine>>::RECEIVE_BUFFERS;

        typedef Ethernet::Address Address;
        typedef Ethernet::Protocol Protocol_Number;
        typedef Buffer<Ethernet::Frame> DataBuffer;
        typedef Conditional_Data_Observer<DataBuffer, Protocol_Number> Observer;
        typedef Conditionally_Data_Observed<DataBuffer, Protocol_Number> Observed;
        
        // Statistics for network operations
        struct Statistics {
            std::atomic<unsigned int> packets_sent;
            std::atomic<unsigned int> packets_received;
            std::atomic<unsigned int> bytes_sent;
            std::atomic<unsigned int> bytes_received;
            std::atomic<unsigned int> tx_drops;
            std::atomic<unsigned int> rx_drops;
            
            Statistics() : 
                packets_sent(0), packets_received(0),
                bytes_sent(0), bytes_received(0),
                tx_drops(0), rx_drops(0) {}
        };

    protected:
        NIC();

    public:
        ~NIC();
        
        // Send data over the network
        // int send(Address dst, Protocol_Number prot, const void* data, unsigned int size);
        
        // Receive data from the network
        // int receive(Address* src, Protocol_Number* prot, void* data, unsigned int size);
        
        // Send a pre-allocated buffer
        int send(DataBuffer* buf);

        // Process a received buffer
        int receive(DataBuffer* buf, Address* src, Address* dst, void* data, unsigned int size);
        
        // Allocate a buffer for sending
        DataBuffer* alloc(Address dst, Protocol_Number prot, unsigned int size);
        
        // Free a buffer after use
        void free(DataBuffer* buf);
        
        // Get the local address
        const Address& address();
        
        // Set the local address
        void setAddress(Address address);
        
        // Get network statistics
        const Statistics& statistics();
        
        // Explicitly stop the NIC and its underlying engine
        void stop() {
            db<NIC>(TRC) << "NIC<Engine>::stop() called! Stopping engine thread...\n";
            
            // First stop the engine thread
            Engine::stop();
            db<NIC>(INF) << "[NIC] Engine thread stopped\n";
            
            // Post to all semaphores to ensure no threads remain blocked on them
            // This is critical to allow component threads to completely terminate
            db<NIC>(TRC) << "[NIC] Unblocking any threads waiting on buffer semaphores\n";
            
            // Determine how many threads might be blocked on the buffer semaphore
            int sem_value;
            sem_getvalue(&_buffer_sem, &sem_value);
            int posts_needed = N_BUFFERS - sem_value;
            
            if (posts_needed > 0) {
                db<NIC>(INF) << "[NIC] Found " << posts_needed << " potentially blocked threads on buffer semaphore\n";
                // Post to semaphores to unblock any waiting threads
                for (int i = 0; i < posts_needed; i++) {
                    sem_post(&_buffer_sem);
                }
            }
            
            // Also unblock any threads waiting on the binary semaphore
            sem_getvalue(&_binary_sem, &sem_value);
            if (sem_value == 0) {
                db<NIC>(INF) << "[NIC] Unblocking binary semaphore\n";
                sem_post(&_binary_sem);
            }
            
            db<NIC>(INF) << "[NIC] All NIC semaphores unblocked\n";
        }
        
        // Attach/detach observers
        // void attach(Observer* obs, Protocol_Number prot);
        // void detach(Observer* obs, Protocol_Number prot);

    private:
        void handleSignal() override;

    private:
        Address _address;
        Statistics _statistics;
        DataBuffer _buffer[N_BUFFERS];
        std::queue<DataBuffer*> _free_buffers;
        sem_t _buffer_sem;
        sem_t _binary_sem;
};

// NIC implementations
template <typename Engine>
NIC<Engine>::NIC() {
    db<NIC>(TRC) << "NIC<Engine>::NIC() called!\n";

    for (unsigned int i = 0; i < N_BUFFERS; ++i) {
        _buffer[i] = DataBuffer();
        _free_buffers.push(&_buffer[i]);
    }
    db<NIC>(INF) << "[NIC] " << std::to_string(N_BUFFERS) << " buffers created\n";

    Engine::start();

    sem_init(&_buffer_sem, 0, N_BUFFERS);
    sem_init(&_binary_sem, 0, 1);

    // Setting default address
    _address = this->_mac_address;
}

template <typename Engine>
NIC<Engine>::~NIC() {
    db<NIC>(TRC) << "NIC<Engine>::~NIC() called!\n";

    // Engine::stop() is now called via _nic->stop() in Vehicle::~Vehicle()
    // so this call is redundant and has been removed
    
    sem_destroy(&_buffer_sem);
    sem_destroy(&_binary_sem);

}

template <typename Engine>
int NIC<Engine>::send(DataBuffer* buf) {
    db<NIC>(TRC) << "NIC<Engine>::send() called!\n";

    // Check if engine is running before trying to send
    if (!Engine::running()) {
        db<NIC>(INF) << "[NIC] send() called while engine is shutting down, dropping packet\n";
        _statistics.tx_drops++;
        free(buf); // Don't leak the buffer
        return -1;
    }

    if (!buf) {
        db<NIC>(INF) << "[NIC] send() requested with null buffer\n";
        _statistics.tx_drops++;
        return -1;
    }

    int result = Engine::send(buf->data(), buf->size());
    db<NIC>(INF) << "[NIC] Engine::send() returned value " << std::to_string(result) << "\n";

    if (result <= 0) {
        _statistics.tx_drops++;
        return -1;
    }

    _statistics.packets_sent++;
    _statistics.bytes_sent += result;
    
    return result;
}

template <typename Engine>
int NIC<Engine>::receive(DataBuffer* buf, Address* src, Address* dst, void* data, unsigned int size) {
    db<NIC>(TRC) << "NIC<Engine>::receive() called!\n";

    // Enhanced validation for buffer
    if (!buf || !buf->data()) {
        db<NIC>(ERR) << "[NIC] receive() called with null buffer or null buffer data\n";
        _statistics.rx_drops++;
        free(buf); // Safe to call our free method
        return -1;
    }

    // Add safety check for unreasonable buffer sizes
    static const unsigned int MAX_EXPECTED_FRAME_SIZE = 1518; // Standard Ethernet max frame size
    if (buf->size() < Ethernet::HEADER_SIZE || buf->size() > MAX_EXPECTED_FRAME_SIZE) {
        db<NIC>(ERR) << "[NIC] receive() called with invalid buffer size: " << buf->size() << "\n";
        _statistics.rx_drops++;
        free(buf); // Safe to call our free method
        return -1;
    }

    if (!data || size == 0) {
        db<NIC>(INF) << "[NIC] receive() requested with null data pointer, or size equals zero\n";
        _statistics.rx_drops++;
        free(buf);
        return -1;
    }

    Ethernet::Frame* frame = buf->data();
    
    // 1. Filling src and dst addresses
    if (src) *src = frame->src;
    if (dst) *dst = frame->dst;
    
    // 2. Payload size
    unsigned int payload_size = buf->size() - Ethernet::HEADER_SIZE;
    db<NIC>(INF) << "[NIC] frame extracted from buffer: {src = " << Ethernet::mac_to_string(frame->src) << ", dst = " << Ethernet::mac_to_string(frame->dst) << ", prot = " << std::to_string(frame->prot) << ", size = " << buf->size() << "}\n";
    
    // Add check for payload size exceeding the provided buffer
    if (payload_size > size) {
        db<NIC>(ERR) << "[NIC] Payload size (" << payload_size << ") exceeds provided buffer size (" << size << ")\n";
        _statistics.rx_drops++;
        free(buf);
        return -2;
    }

    // 3. Copies packet to data pointer
    std::memcpy(data, frame->payload, payload_size);

    // 4. Releases the buffer
    free(buf);

    // 5. Return size of copied bytes
    return payload_size;
}

template <typename Engine>
void NIC<Engine>::handleSignal() {
    db<SocketEngine>(TRC) << "SocketEngine::handleSignal() called!\n";
    
    // Early check - if engine is no longer running, don't process packets
    if (!Engine::running()) {
        db<SocketEngine>(TRC) << "[SocketEngine] Engine no longer running, ignoring signal\n";
        return;
    }
    
    Ethernet::Frame frame;
    struct sockaddr_ll src_addr;
    socklen_t addr_len = sizeof(src_addr);
    
    int bytes_received = recvfrom(this->_sock_fd, &frame, sizeof(frame), 0, reinterpret_cast<sockaddr*>(&src_addr), &addr_len);
                               
    if (bytes_received < 0) {
        db<SocketEngine>(INF) << "[SocketEngine] No data received\n";
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("recvfrom");
        }
        return;
    }
    
    // Check again - if engine was stopped during recvfrom, don't continue processing
    if (!Engine::running()) {
        db<SocketEngine>(TRC) << "[SocketEngine] Engine stopped during receive, discarding frame\n";
        return;
    }
    
    // Check for valid Ethernet frame size (at least header size)
    if (static_cast<unsigned int>(bytes_received) < Ethernet::HEADER_SIZE) {
        db<SocketEngine>(ERR) << "[SocketEngine] Received undersized frame (" << bytes_received << " bytes)\n";
        return;
    }
    
    // Convert protocol from network to host byte order
    frame.prot = ntohs(frame.prot);
    // // Filters out messages from itself
    if (_address == frame.src) return;
    db<SocketEngine>(INF) << "[SocketEngine] received frame: {src = " << Ethernet::mac_to_string(frame.src) << ", dst = " << Ethernet::mac_to_string(frame.dst) << ", prot = " << frame.prot << ", size = " << bytes_received << "}\n";
    
    // Process the frame if callback is set
    // 1. Extracting header
    Ethernet::Address dst = frame.dst;
    Ethernet::Protocol proto = frame.prot;

    // 2. Allocate buffer
    DataBuffer * buf = alloc(dst, proto, bytes_received);
    if (!buf) return;

    // 4. Copy frame to buffer
    std::memcpy(buf->data(), &frame, bytes_received);

    // 5. Notify Observers
    if (!notify(proto, buf)) {
        db<NIC>(INF) << "[NIC] data received, but no one was notified " << proto << "\n";
        free(buf); // if no one is listening, free buffer
    }
}

template <typename Engine>
typename NIC<Engine>::DataBuffer* NIC<Engine>::alloc(Address dst, Protocol_Number prot, unsigned int size) {
    db<NIC>(TRC) << "NIC<Engine>::alloc() called!\n";

    // Special handling for test environment
    #ifdef TEST_MODE
    // In test mode, we still allow allocation even when engine is stopped
    if (!Engine::running()) {
        db<NIC>(INF) << "[NIC] Test mode: Allowing allocation despite engine being stopped\n";
    }
    #else
    // For normal operation, check if engine is still running before trying to allocate
    if (!Engine::running()) {
        db<NIC>(INF) << "[NIC] alloc() called while engine is shutting down, returning nullptr\n";
        _statistics.tx_drops++;
        return nullptr;
    }
    #endif

    // Non-blocking attempt to get a buffer semaphore
    if (sem_trywait(&_buffer_sem) != 0) {
        // If we can't get a buffer immediately during normal operation
        #ifndef TEST_MODE
        if (Engine::running()) {
        #endif
            // During normal operation, block until a buffer is available
            db<NIC>(TRC) << "[NIC] No buffers immediately available, waiting...\n";
            
            // Try the semaphore with a timeout to avoid deadlock during shutdown
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 1; // 1 second timeout
            
            if (sem_timedwait(&_buffer_sem, &ts) != 0) {
                // If we timeout or get interrupted, check if we're shutting down
                if (!Engine::running()) {
                    db<NIC>(INF) << "[NIC] Timed out waiting for buffer during shutdown\n";
                    _statistics.tx_drops++;
                    return nullptr;
                }
                // Otherwise it's a genuine error (should rarely happen)
                if (errno != ETIMEDOUT) {
                    db<NIC>(ERR) << "[NIC] Error waiting for buffer: " << strerror(errno) << "\n";
                }
                _statistics.tx_drops++;
                return nullptr;
            }
        #ifndef TEST_MODE
        } else {
            // During shutdown, don't block - just report failure
            db<NIC>(INF) << "[NIC] No buffers available during shutdown, returning nullptr\n";
            _statistics.tx_drops++;
            return nullptr;
        }
        #endif
    }
    
    #ifndef TEST_MODE
    // Check again if engine is still running after we got the semaphore
    if (!Engine::running()) {
        db<NIC>(INF) << "[NIC] Engine stopped after buffer allocation started, releasing semaphore\n";
        sem_post(&_buffer_sem); // Return the semaphore back to the pool
        _statistics.tx_drops++;
        return nullptr;
    }
    #endif
    
    // Now get a buffer from the queue with the binary semaphore
    if (sem_trywait(&_binary_sem) != 0) {
        // If we can't get the binary semaphore immediately, check if we're in shutdown
        if (!Engine::running()) {
            db<NIC>(INF) << "[NIC] Unable to acquire binary semaphore during shutdown\n";
            sem_post(&_buffer_sem); // Return the buffer semaphore
            _statistics.tx_drops++;
            return nullptr;
        }
        
        // In normal operation, wait for the binary semaphore
        sem_wait(&_binary_sem);
    }
    
    // Final check - make sure we have buffers available
    if (_free_buffers.empty()) {
        db<NIC>(ERR) << "[NIC] Buffer queue empty despite semaphore, inconsistent state\n";
        sem_post(&_binary_sem);
        sem_post(&_buffer_sem);
        _statistics.tx_drops++;
        return nullptr;
    }
    
    DataBuffer* buf = _free_buffers.front();
    _free_buffers.pop();
    sem_post(&_binary_sem);

    buf->data()->src = address();
    buf->data()->dst = dst;
    buf->data()->prot = prot;
    buf->setSize(size);

    db<NIC>(INF) << "[NIC] buffer allocated for frame: {src = " << Ethernet::mac_to_string(address()) << ", dst = " << Ethernet::mac_to_string(dst) << ", prot = " << std::to_string(prot) << ", size = " << size << "}\n";

    return buf;
}

template <typename Engine>
void NIC<Engine>::free(DataBuffer* buf) {
    db<NIC>(TRC) << "NIC<Engine>::free() called!\n";

    if (!buf) {
        db<NIC>(WRN) << "[NIC] Attempted to free null buffer\n";
        return;
    }

    buf->clear();
    db<NIC>(INF) << "[NIC] buffer released\n";

    // Special handling for test environment
    #ifdef TEST_MODE
    // In test mode, we always use the blocking semaphore operations
    sem_wait(&_binary_sem);
    _free_buffers.push(buf);
    sem_post(&_binary_sem);
    sem_post(&_buffer_sem);
    #else
    // For normal operation, use try-wait to avoid potential deadlocks during shutdown
    if (sem_trywait(&_binary_sem) == 0) {
        _free_buffers.push(buf);
        sem_post(&_binary_sem);
        sem_post(&_buffer_sem);
    } else {
        // If we can't get the binary semaphore immediately, check if we're shutting down
        if (!Engine::running()) {
            db<NIC>(WRN) << "[NIC] Unable to return buffer to pool during shutdown\n";
            // Don't wait for semaphore during shutdown - just accept the leak
            return;
        }
        
        // During normal operation, we should still block to ensure proper buffer management
        sem_wait(&_binary_sem);
        _free_buffers.push(buf);
        sem_post(&_binary_sem);
        sem_post(&_buffer_sem);
    }
    #endif
}

template <typename Engine>
const typename NIC<Engine>::Address& NIC<Engine>::address() {
    db<NIC>(TRC) << "NIC<Engine>::address() called!\n";

    return _address;
}

template <typename Engine>
void NIC<Engine>::setAddress(Address address) {
    db<NIC>(TRC) << "NIC<Engine>::setAddress() called!\n";

    _address = address;
    db<NIC>(INF) << "[NIC] address setted: " << Ethernet::mac_to_string(address) << "\n";
}

template <typename Engine>
const typename NIC<Engine>::Statistics& NIC<Engine>::statistics() {
    db<NIC>(TRC) << "NIC<Engine>::statistics() called!\n";

    return _statistics;
}

#endif // NIC_H