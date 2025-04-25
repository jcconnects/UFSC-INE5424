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
        void stop();
        
        // Attach/detach observers
        // void attach(Observer* obs, Protocol_Number prot); // inherited
        // void detach(Observer* obs, Protocol_Number prot); // inherited

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

    
    sem_init(&_buffer_sem, 0, N_BUFFERS);
    sem_init(&_binary_sem, 0, 1);
    
    // Setting default address
    _address = this->_mac_address;
    
    // Starting Engine
    Engine::start();
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

    // Check for valid Ethernet frame size (at least header size)
    if (static_cast<unsigned int>(bytes_received) < Ethernet::HEADER_SIZE) {
        db<SocketEngine>(ERR) << "[SocketEngine] Received undersized frame (" << bytes_received << " bytes)\n";
        return;
    }
    
    // Convert protocol from network to host byte order
    frame.prot = ntohs(frame.prot);
    // // Filters out messages from itself
    if (_address == frame.src || (frame.dst != _address && frame.dst != Ethernet::BROADCAST)) {
        db<SocketEngine>(INF) << "[SocketEngine] Ignoring frame from self or not for this NIC\n";
        return;
    }
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

    sem_wait(&_buffer_sem);

    if (!Engine::running()) {
        db<NIC>(WRN) << "[NIC] alloc() called when NIC has finished\n";
        sem_post(&_buffer_sem);
        return nullptr;
    }
    
    sem_wait(&_binary_sem);
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

    if (!buf) return;

    buf->clear();
    db<NIC>(INF) << "[NIC] buffer released\n";

    sem_wait(&_binary_sem);
    _free_buffers.push(buf);
    sem_post(&_binary_sem);

    sem_post(&_buffer_sem);
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


template <typename Engine>
void NIC<Engine>::stop() {
    db<NIC>(TRC) << "NIC<Engine>::stop() called!\n";

    // Stops the engine execution
    Engine::stop();
    db<NIC>(INF) << "[NIC] Engine stopped\n";
}
#endif // NIC_H