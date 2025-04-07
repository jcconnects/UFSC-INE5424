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
        
        // Attach/detach observers
        // void attach(Observer* obs, Protocol_Number prot);
        // void detach(Observer* obs, Protocol_Number prot);

    private:
        void receiveData(Ethernet::Frame& frame, unsigned int size);

    private:
        Statistics _statistics;
        DataBuffer _buffer[N_BUFFERS];
        std::queue<DataBuffer*> _free_buffers;
        sem_t _buffer_sem;
        pthread_mutex_t _buffer_mtx;
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

    sem_init(&_buffer_sem, 0, BUFFER_SIZE);
    pthread_mutex_init(&_buffer_mtx, nullptr);
    this->setCallback(std::bind(&NIC::receiveData, this, std::placeholders::_1, std::placeholders::_2));
}

template <typename Engine>
NIC<Engine>::~NIC() {
    db<NIC>(TRC) << "NIC<Engine>::~NIC() called!\n";

    Engine::stop();
    
    sem_destroy(&_buffer_sem);
    pthread_mutex_destroy(&_buffer_mtx);

}

template <typename Engine>
int NIC<Engine>::send(DataBuffer* buf) {
    db<NIC>(TRC) << "NIC<Engine>::send() called!\n";

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

    if (!buf || !data || size == 0) {
        db<NIC>(INF) << "[NIC] receive() requested with null buffer, null data pointer, or size equals zero\n";
        _statistics.rx_drops++;
        return -1;
    }

    Ethernet::Frame* frame = buf->data();
    
    // 1. Filling src and dst addresses
    if (src) *src = frame->src;
    if (dst) *dst = frame->dst;
    
    // 2. Payload size
    unsigned int payload_size = buf->size() - Ethernet::HEADER_SIZE; // tamanho total do frame
    db<NIC>(INF) << "[NIC] frame extracted from buffer: {src = " << Ethernet::mac_to_string(frame->src) << ", dst = " << Ethernet::mac_to_string(frame->dst) << ", prot = " << std::to_string(frame->prot) << ", size = " << buf->size() << "}\n";
    
    // 3. Copies packet to data pointer
    std::memcpy(data, frame->payload, payload_size);

    // 4. Releases the buffer
    free(buf);

    // 5. Return size of copied bytes
    return payload_size;
}

template <typename Engine>
void NIC<Engine>::receiveData(Ethernet::Frame& frame, unsigned int size) {
    db<NIC>(TRC) << "NIC<Engine>::receiveData() called!\n";
    
    // 1. Extracting header
    Ethernet::Address dst = frame.dst;
    Ethernet::Protocol proto = frame.prot;

    // 2. Allocate buffer
    DataBuffer * buf = alloc(dst, proto, size);
    if (!buf) return;

    // 4. Copy frame to buffer
    std::memcpy(buf->data(), &frame, size);

    // 5. Notify Observers
    if (!notify(proto, buf)) {
        db<NIC>(INF) << "[NIC] data received, but no one was notified " << proto << "\n";
        free(buf); // if no one is listening, free buffer
    }
}

template <typename Engine>
typename NIC<Engine>::DataBuffer* NIC<Engine>::alloc(Address dst,   Protocol_Number prot, unsigned int size) {
    db<NIC>(TRC) << "NIC<Engine>::alloc() called!\n";

    sem_wait(&_buffer_sem);
    
    pthread_mutex_lock(&_buffer_mtx);
    DataBuffer* buf = _free_buffers.front();
    _free_buffers.pop();
    pthread_mutex_unlock(&_buffer_mtx);

    if (buf == nullptr) {
        // Option 2: Return failure indicator (e.g., nullptr)
        std::cerr << "Warning/Error: NIC::alloc failed, no free buffers available." << std::endl;
        return nullptr; // Or throw an exception
    }

    Ethernet::Frame init_frame;
    init_frame.src = {};
    init_frame.src = {};
    init_frame.prot = 0;
    std::memset(&init_frame, 0, Ethernet::MTU);
    buf -> setData(&init_frame, sizeof(Ethernet::Frame));

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

    if (!buf) return;

    buf->clear();
    db<NIC>(INF) << "[NIC] buffer released\n";

    pthread_mutex_lock(&_buffer_mtx);
    _free_buffers.push(buf);
    pthread_mutex_unlock(&_buffer_mtx);

    sem_post(&_buffer_sem);
}

template <typename Engine>
const typename NIC<Engine>::Address& NIC<Engine>::address() {
    db<NIC>(TRC) << "NIC<Engine>::address() called!\n";

    return this->_address;
}

template <typename Engine>
void NIC<Engine>::setAddress(Address address) {
    db<NIC>(TRC) << "NIC<Engine>::setAddress() called!\n";

    this->_address = address;
}

template <typename Engine>
const typename NIC<Engine>::Statistics& NIC<Engine>::statistics() {
    db<NIC>(TRC) << "NIC<Engine>::statistics() called!\n";

    return _statistics;
}

#endif // NIC_H