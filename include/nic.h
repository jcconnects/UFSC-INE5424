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
template <typename ExternalEngine, typename InternalEngine>
class NIC: public Ethernet, public Conditionally_Data_Observed<Buffer<Ethernet::Frame>, Ethernet::Protocol>, private ExternalEngine, private InternalEngine
{
    friend class Initializer;

    public:
        static const unsigned int N_BUFFERS = Traits<NIC<ExternalEngine, InternalEngine>>::SEND_BUFFERS + Traits<NIC<ExternalEngine, InternalEngine>>::RECEIVE_BUFFERS;
        static constexpr unsigned int MAX_FRAME_SIZE = sizeof(Ethernet::Frame);

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
        
        // Send a pre-allocated buffer
        int send(DataBuffer* buf);

        // Process a received buffer
        int receive(DataBuffer* buf, Address* src, Address* dst, void* data, unsigned int size);
        
        // Allocate a buffer for sending
        DataBuffer* alloc(Address dst, Protocol_Number prot, unsigned int size);
        
        // Release a buffer after use
        void free(DataBuffer* buf);
        
        // Get the local address
        const Address& address();
        
        // Set the local address
        void setAddress(Address address);
        
        // Get network statistics
        const Statistics& statistics();
        
        // Explicitly stop the NIC and its underlying ExternalEngine
        void stop();
        
        // Return wheater NIC is still active or not
        const bool running();

        // Attach/detach observers
        // void attach(Observer* obs, Protocol_Number prot); // inherited
        // void detach(Observer* obs, Protocol_Number prot); // inherited

    private:
        void handleExternal(Ethernet::Frame* frame, unsigned int size) override;
        void handleInternal(DataBuffer* buf) override;

    private:

        Address _address;
        Statistics _statistics;
        DataBuffer _buffer[N_BUFFERS];
        std::queue<DataBuffer*> _free_buffers;
        std::atomic<bool> _running;
        sem_t _buffer_sem;
        sem_t _binary_sem;
};

/*********** NIC Implementation ************/
template <typename ExternalEngine, typename InternalEngine>
NIC<ExternalEngine, InternalEngine>::NIC() {
    db<NIC>(TRC) << "NIC<ExternalEngine, InternalEngine>::NIC() called!\n";

    for (unsigned int i = 0; i < N_BUFFERS; ++i) {
        _buffer[i] = DataBuffer();
        _free_buffers.push(&_buffer[i]);
    }
    db<NIC>(INF) << "[NIC] " << std::to_string(N_BUFFERS) << " buffers created\n";

    
    sem_init(&_buffer_sem, 0, N_BUFFERS);
    sem_init(&_binary_sem, 0, 1);
    
    // Starting Engines
    ExternalEngine::start();
    InternalEngine::start();

    // Setting default address
    _address = ExternalEngine::mac_address();
    
    // Activating NIC
    _running = true;
}

template <typename ExternalEngine, typename InternalEngine>
NIC<ExternalEngine, InternalEngine>::~NIC() {
    db<NIC>(TRC) << "NIC<ExternalEngine, InternalEngine>::~NIC() called!\n";

    // ExternalEngine::stop() is now called via _nic->stop() in Vehicle::~Vehicle()
    // so this call is redundant and has been removed

    sem_destroy(&_buffer_sem);
    sem_destroy(&_binary_sem);

}

template <typename ExternalEngine, typename InternalEngine>
int NIC<ExternalEngine, InternalEngine>::send(DataBuffer* buf) {
    db<NIC>(TRC) << "NIC<ExternalEngine, InternalEngine>::send() called!\n";

    // Check if ExternalEngine is running before trying to send
    if (!running()) {
        db<NIC>(INF) << "[NIC] send() called when NIC is deactivated\n";
        _statistics.tx_drops++;
        return -1;
    }

    if (!buf || !buf->data()) {
        db<NIC>(ERR) << "[NIC] send() called with null buffer\n";
        _statistics.tx_drops++;
        return -1;
    }

    // Send logic
    int result = -1;

    /*********** INTERNAL SEND ************/
    if (buf->data()->dst == _address) {
        db<NIC>(INF) << "[NIC] Routing frame locally via InternalEngine (dst == self)\n";
        result = InternalEngine::send(buf);
    /********** EXTERNAL SEND *************/
    } else {
        db<NIC>(INF) << "[NIC] Routing frame externally via ExternalEngine (dst != self)\n";
        result = ExternalEngine::send(buf->data(), buf->size());
        free(buf); // Release buffer only for external usage
    }

    if (result <= 0) {
        _statistics.tx_drops++;
    } else {
        _statistics.packets_sent++;
        _statistics.bytes_sent += result;
    }
    
    return result;
}

template <typename ExternalEngine, typename InternalEngine>
int NIC<ExternalEngine, InternalEngine>::receive(DataBuffer* buf, Address* src, Address* dst, void* data, unsigned int size) {
    db<NIC>(TRC) << "NIC<ExternalEngine, InternalEngine>::receive() called!\n";

    // Check weather buffer is null or empty
    if (!buf || !buf->data()) {
        db<NIC>(ERR) << "[NIC] receive() called with null buffer or null buffer data\n";
        _statistics.rx_drops++;
        free(buf); // Safe to release buffer
        return -1;
    }

    // Add safety check for unreasonable buffer sizes
    if (buf->size() < Ethernet::HEADER_SIZE || buf->size() > MAX_FRAME_SIZE) {
        db<NIC>(ERR) << "[NIC] receive() called with invalid buffer size: " << buf->size() << "\n";
        _statistics.rx_drops++;
        free(buf); // Safe to release buffer
        return -1;
    }

    // Check weather data is null or size equals zero
    if (!data || size == 0) {
        db<NIC>(INF) << "[NIC] receive() called with null data pointer, or size equals zero\n";
        _statistics.rx_drops++;
        free(buf); // Safe to release buffer
        return -1;
    }

    Ethernet::Frame* frame = buf->data();
    db<NIC>(INF) << "[NIC] frame extracted from buffer: {src = " << Ethernet::mac_to_string(frame->src) << ", dst = " << Ethernet::mac_to_string(frame->dst) << ", prot = " << std::to_string(frame->prot) << ", size = " << buf->size() << "}\n";
    
    // 1. Filling src and dst addresses
    if (src) *src = frame->src;
    if (dst) *dst = frame->dst;
    
    // 2. Payload size
    unsigned int payload_size = buf->size() - Ethernet::HEADER_SIZE;
    
    // Checks weather payload size exceeds provided data pointer size
    if (payload_size > size) {
        db<NIC>(ERR) << "[NIC] Payload size (" << payload_size << ") exceeds provided data pointer size (" << size << ")\n";
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

template <typename ExternalEngine, typename InternalEngine>
void NIC<ExternalEngine, InternalEngine>::handleExternal(Ethernet::Frame* frame, unsigned int size) {
    db<NIC>(TRC) << "NIC::handleExternal() called!\n";
    
    // Checks weather NIC is still active
    if (!running()) {
        db<NIC>(WRN) << "[NIC] handleExternal called when NIC is deactivated\n";
        return;
    }

    // Filter check (optional, engine might do this): ignore packets sent by self
    if (frame->src == _address) {
        db<NIC>(INF) << "[NIC] Ignoring frame from self: {src=" << Ethernet::mac_to_string(frame->src) << "}\n";
        return;
   }

   // Filter check: ignore packets not for us or broadcast
   if (frame->dst != _address && frame->dst != Ethernet::BROADCAST) {
        db<NIC>(INF) << "[NIC] Ignoring frame not for this NIC: {dst=" << Ethernet::mac_to_string(frame->dst) << "}\n";
        return;
    }

    // 1. Extracting frame header
    Ethernet::Address dst = frame->dst;
    Ethernet::Protocol proto = frame->prot;

    // 2. Allocate buffer
    unsigned int packet_size = size - Ethernet::HEADER_SIZE;
    DataBuffer * buf = alloc(dst, proto, packet_size);
    if (!buf) return;

    // 4. Copy frame to buffer
    buf->setData(frame, size);

    // 5. Notify Observers
    if (!notify(proto, buf)) {
        db<NIC>(INF) << "[NIC] data received, but no one was notified " << proto << "\n";
        free(buf); // if no one is listening, release buffer
    }
}

template<typename ExternalEngine, typename InternalEngine>
void NIC<ExternalEngine, InternalEngine>::handleInternal(DataBuffer* buf) {
    db<NIC>(TRC) << "NIC::handleInternal() called!\n";

    // Checks weather NIC is still active
    if (!running()) {
        db<NIC>(WRN) << "[NIC] handleInternal called when NIC is deactivated\n";
        return;
    }

    // Extracting frame from buffer
    Ethernet::Frame* frame = buf->data();
    unsigned int proto = frame->prot;

    // Notify observers
    if (!notify(proto, buf)) {
        db<NIC>(INF) << "[NIC] data received, but no one was notified " << proto << "\n";
        free(buf); // if no one is listening, release buffer
    }
}

template <typename ExternalEngine, typename InternalEngine>
typename NIC<ExternalEngine, InternalEngine>::DataBuffer* NIC<ExternalEngine, InternalEngine>::alloc(Address dst, Protocol_Number prot, unsigned int size) {
    db<NIC>(TRC) << "NIC<ExternalEngine, InternalEngine>::alloc() called!\n";
    
    // Acquire free buffers counter semaphore
    sem_wait(&_buffer_sem);

    // Check weather NIC is still active
    if (!running()) {
        db<NIC>(WRN) << "[NIC] alloc() called when NIC is deactivated\n";
        sem_post(&_buffer_sem);
        return nullptr;
    }
    
    // Remove first buffer of the free buffers queue
    sem_wait(&_binary_sem);
    DataBuffer* buf = _free_buffers.front();
    _free_buffers.pop();
    sem_post(&_binary_sem);

    // Set Frame
    Ethernet::Frame frame;
    frame.src = address();
    frame.dst = dst;
    frame.prot = prot;
    unsigned int frame_size = size + Ethernet::HEADER_SIZE;

    // Set buffer data
    buf->setData(&frame, frame_size);

    db<NIC>(INF) << "[NIC] buffer allocated for frame: {src = " << Ethernet::mac_to_string(frame.src) << ", dst = " << Ethernet::mac_to_string(frame.dst) << ", prot = " << std::to_string(frame.prot) << ", size = " << buf->size() << "}\n";

    return buf;
}

template <typename ExternalEngine, typename InternalEngine>
void NIC<ExternalEngine, InternalEngine>::free(DataBuffer* buf) {
    db<NIC>(TRC) << "NIC<ExternalEngine, InternalEngine>::free() called!\n";

    if (!buf) {
        db<NIC>(ERR) << "[NIC] free() called with null buffer!\n";
        return;
    };

    // Clear buffer
    buf->clear();
    
    // Add to free buffers queue
    sem_wait(&_binary_sem);
    _free_buffers.push(buf);
    sem_post(&_binary_sem);
    
    // Increase free buffers counter
    sem_post(&_buffer_sem);

    db<NIC>(INF) << "[NIC] buffer released\n";
}

template <typename ExternalEngine, typename InternalEngine>
void NIC<ExternalEngine, InternalEngine>::stop() {
    db<NIC>(TRC) << "NIC<ExternalEngine, InternalEngine>::stop() called!\n";

    if (!running()) {
        db<NIC>(WRN) << "[NIC] stop called when NIC is deactivated\n";
        return;
    }

    // Stop any NIC operations
    _running.store(false, std::memory_order_release);

    // Stops both Engines
    ExternalEngine::stop();
    InternalEngine::stop();

    db<NIC>(INF) << "[NIC] All engines stopped\n";
}

template <typename ExternalEngine, typename InternalEngine>
const typename NIC<ExternalEngine, InternalEngine>::Address& NIC<ExternalEngine, InternalEngine>::address() {
    return _address;
}

template <typename ExternalEngine, typename InternalEngine>
void NIC<ExternalEngine, InternalEngine>::setAddress(Address address) {
    db<NIC>(TRC) << "NIC<ExternalEngine, InternalEngine>::setAddress() called!\n";

    _address = address;
    db<NIC>(INF) << "[NIC] address setted: " << Ethernet::mac_to_string(address) << "\n";
}

template<typename ExternalEngine, typename InternalEngine>
const bool NIC<ExternalEngine, InternalEngine>::running() {
    return _running.load(std::memory_order_acquire);
}

template <typename ExternalEngine, typename InternalEngine>
const typename NIC<ExternalEngine, InternalEngine>::Statistics& NIC<ExternalEngine, InternalEngine>::statistics() {
    return _statistics;
}

#endif // NIC_H