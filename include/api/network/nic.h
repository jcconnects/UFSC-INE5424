#ifndef NIC_H
#define NIC_H

#include <semaphore.h>
#include <pthread.h>
#include <queue>

#include "api/traits.h"
#include "api/network/ethernet.h"
#include "api/util/debug.h"
#include "api/util/observer.h"
#include "api/util/observed.h"
#include "api/util/buffer.h"

// Foward Declaration
class Initializer;

// Network Interface Card implementation
template <typename Engine>
class NIC: public Ethernet, public Conditionally_Data_Observed<Buffer<Ethernet::Frame>, Ethernet::Protocol>, private Engine
{
    friend class Initializer;

    public:
        static const unsigned int N_BUFFERS = Traits<NIC<Engine>>::SEND_BUFFERS + Traits<NIC<Engine>>::RECEIVE_BUFFERS;
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
        
        // Attach/detach observers
        // void attach(Observer* obs, Protocol_Number prot); // inherited
        // void detach(Observer* obs, Protocol_Number prot); // inherited

    private:
        void handle(Ethernet::Frame* frame, unsigned int size) override;

    private:

        Address _address;
        Statistics _statistics;
        DataBuffer _buffer[N_BUFFERS];
        std::queue<DataBuffer*> _free_buffers;
        sem_t _buffer_sem;
        sem_t _binary_sem;
};

/*********** NIC Implementation ************/
template <typename Engine>
NIC<Engine>::NIC() {
    // Engine starts on its own

    for (unsigned int i = 0; i < N_BUFFERS; ++i) {
        _buffer[i] = DataBuffer();
        _free_buffers.push(&_buffer[i]);
    }
    db<NIC>(INF) << "[NIC] " << std::to_string(N_BUFFERS) << " buffers created\n";
    
    sem_init(&_buffer_sem, 0, N_BUFFERS);
    sem_init(&_binary_sem, 0, 1);

    // Setting default address
    _address = Engine::mac_address();

}

template <typename Engine>
NIC<Engine>::~NIC() {
    sem_destroy(&_buffer_sem);
    sem_destroy(&_binary_sem);
    db<NIC>(INF) << "[NIC] semaphores destroyed\n";
    // Engine stops itself on its own
}

template <typename Engine>
int NIC<Engine>::send(DataBuffer* buf) {
    db<NIC>(TRC) << "NIC<Engine>::send() called!\n";

    int result = Engine::send(buf->data(), buf->size());
    db<NIC>(INF) << "[NIC] Engine::send returned " << result << "\n";

    if (result <= 0) {
        _statistics.tx_drops++;
        result = 0;
    } else {
        _statistics.packets_sent++;
        _statistics.bytes_sent += result;
    }
    
    return result;
}

template <typename Engine>
int NIC<Engine>::receive(DataBuffer* buf, Address* src, Address* dst, void* data, unsigned int size) {
    db<NIC>(TRC) << "NIC<Engine>::receive() called!\n";

    Ethernet::Frame* frame = buf->data();
    db<NIC>(INF) << "[NIC] frame extracted from buffer: {src = " << Ethernet::mac_to_string(frame->src) << ", dst = " << Ethernet::mac_to_string(frame->dst) << ", prot = " << std::to_string(frame->prot) << ", size = " << buf->size() << "}\n";
    
    // 1. Filling src and dst addresses
    if (src) *src = frame->src;
    if (dst) *dst = frame->dst;
    
    // 2. Payload size
    unsigned int payload_size = buf->size() - Ethernet::HEADER_SIZE;

    // 3. Copies payload to data pointer
    std::memcpy(data, frame->payload, payload_size);

    // 4. Releases the buffer
    free(buf);

    // 5. Return size of copied bytes
    return payload_size;
}

template <typename Engine>
void NIC<Engine>::handle(Ethernet::Frame* frame, unsigned int size) {
    db<NIC>(TRC) << "NIC::handle() called!\n";

    // Filter check (optional, engine might do this): ignore packets sent by self
    if (frame->src == _address) {
        db<NIC>(INF) << "[NIC] ignoring frame from self: {src=" << Ethernet::mac_to_string(frame->src) << "}\n";
        return;
   }

    // 1. Extracting frame header
    Ethernet::Address dst = frame->dst;
    Ethernet::Protocol proto = frame->prot;

    // 2. Allocate buffer
    unsigned int packet_size = size - Ethernet::HEADER_SIZE;
    if (packet_size == 0) {
        db<NIC>(INF) << "[NIC] dropping empty frame\n";
        _statistics.rx_drops++;
        return;
    }

    DataBuffer * buf = alloc(dst, proto, packet_size);

    // 3. Copy frame to buffer
    buf->setData(frame, size);

    // 4. Notify Observers
    if (!notify(buf, proto)) {
        db<NIC>(INF) << "[NIC] data received, but no one was notified (" << proto << ")\n";
        free(buf); // if no one is listening, release buffer
    }
}

template <typename Engine>
typename NIC<Engine>::DataBuffer* NIC<Engine>::alloc(Address dst, Protocol_Number prot, unsigned int size) {
    db<NIC>(TRC) << "NIC<Engine>::alloc() called!\n";
    
    // Acquire free buffers counter semaphore
    sem_wait(&_buffer_sem);
    
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

template <typename Engine>
void NIC<Engine>::free(DataBuffer* buf) {
    db<NIC>(TRC) << "NIC<Engine>::free() called!\n";

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

template <typename Engine>
const typename NIC<Engine>::Address& NIC<Engine>::address() {
    return _address;
}

template <typename Engine>
void NIC<Engine>::setAddress(Address address) {
    _address = address;
    db<NIC>(INF) << "[NIC] address setted: " << Ethernet::mac_to_string(address) << "\n";
}

template <typename Engine>
const typename NIC<Engine>::Statistics& NIC<Engine>::statistics() {
    return _statistics;
}

#endif // NIC_H