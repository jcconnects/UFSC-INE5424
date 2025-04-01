#ifndef NIC_H
#define NIC_H

#include "observer.h"
#include "observed.h"
#include "ethernet.h"
#include "traits.h"
#include "buffer.h"
#include <semaphore.h>
#include <pthread.h>
#include <queue>
#include <atomic>

// Network Interface Card implementation
template <typename Engine>
class NIC: public Ethernet, public Conditionally_Data_Observed<Buffer<Ethernet::Frame>, Ethernet::Protocol>, private Engine
{
public:
    static const unsigned int BUFFER_SIZE =
        Traits<NIC<Engine>>::SEND_BUFFERS * sizeof(Buffer<Ethernet::Frame>) +
        Traits<NIC<Engine>>::RECEIVE_BUFFERS * sizeof(Buffer<Ethernet::Frame>);
    
    typedef Ethernet::Address Address;
    typedef Ethernet::Protocol Protocol_Number;
    typedef Buffer<Ethernet::Frame> Buffer;
    typedef Conditional_Data_Observer<Buffer, Ethernet::Protocol> Observer;
    typedef Conditionally_Data_Observed<Buffer, Ethernet::Protocol> Observed;
    
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
    int send(Address dst, Protocol_Number prot, const void* data, unsigned int size);
    
    // Receive data from the network
    int receive(Address* src, Protocol_Number* prot, void* data, unsigned int size);
    
    // Process a received buffer
    int receive(Buffer* buf, Address* src, Address* dst, void* data, unsigned int size);
    
    // Allocate a buffer for sending
    Buffer* alloc(Address dst, Protocol_Number prot, unsigned int size);
    
    // Send a pre-allocated buffer
    int send(Buffer* buf);
    
    // Free a buffer after use
    void free(Buffer* buf);
    
    // Get the local address
    const Address& address();
    
    // Set the local address
    void address(Address address);
    
    // Get network statistics
    const Statistics& statistics();
    
    // Attach/detach observers
    void attach(Observer* obs, Protocol_Number prot);
    void detach(Observer* obs, Protocol_Number prot);

private:
    Statistics _statistics;
    Buffer _buffer[BUFFER_SIZE];
    std::queue<Buffer*> _free_buffers;
    sem_t _buffer_sem;
    pthread_mutex_t _buffer_mtx;
};

// NIC implementations
template <typename Engine>
NIC<Engine>::NIC() {
    for (unsigned int i = 0; i < BUFFER_SIZE; ++i) {
        _buffer[i] = Buffer();
        _free_buffers.push(&_buffer[i]);
    }

    sem_init(&_buffer_sem, 0, BUFFER_SIZE);
    pthread_mutex_init(&_buffer_mtx, nullptr);
}

template <typename Engine>
NIC<Engine>::~NIC() {
    sem_destroy(&_buffer_sem);
    pthread_mutex_destroy(&_buffer_mtx);
}

template <typename Engine>
int NIC<Engine>::send(Address dst, Protocol_Number prot, const void* data, unsigned int size) {
    if (!data || size == 0) {
        _statistics.tx_drops++;
        return -1;
    }

    Buffer* buf = alloc(dst, prot, size);
    if (!buf) {
        _statistics.tx_drops++;
        return -1;
    }

    // Copy data to buffer
    memcpy(buf->data(), data, size);
    buf->setSize(size);

    int result = send(buf);
    if (result <= 0) {
        _statistics.tx_drops++;
        free(buf);
        return -1;
    }

    _statistics.packets_sent++;
    _statistics.bytes_sent += result;
    return result;
}

template <typename Engine>
int NIC<Engine>::receive(Address* src, Protocol_Number* prot, void* data, unsigned int size) {
    if (!data || size == 0) {
        _statistics.rx_drops++;
        return -1;
    }

    Buffer* buf = alloc(Address(), 0, size);
    if (!buf) {
        _statistics.rx_drops++;
        return -1;
    }

    int result = Engine::receive(buf->data(), size);
    if (result <= 0) {
        _statistics.rx_drops++;
        free(buf);
        return -1;
    }

    buf->setSize(result);
    Ethernet::Frame* frame = buf->data();
    
    if (src) *src = frame->src;
    if (prot) *prot = frame->prot;
    
    memcpy(data, frame->payload, result);
    
    _statistics.packets_received++;
    _statistics.bytes_received += result;
    
    free(buf);
    return result;
}

template <typename Engine>
int NIC<Engine>::receive(Buffer* buf, Address* src, Address* dst, void* data, unsigned int size) {
    if (!buf || !data || size == 0) {
        _statistics.rx_drops++;
        return -1;
    }

    Ethernet::Frame* frame = buf->data();
    
    if (src) *src = frame->src;
    if (dst) *dst = frame->dst;
    
    size_t copy_size = std::min<size_t>(size, buf->size() - sizeof(Ethernet::Frame));
    memcpy(data, frame->payload, copy_size);
    
    return copy_size;
}

template <typename Engine>
typename NIC<Engine>::Buffer* NIC<Engine>::alloc(Address dst, Protocol_Number prot, unsigned int size) {
    sem_wait(&_buffer_sem);

    pthread_mutex_lock(&_buffer_mtx);
    if (_free_buffers.empty()) {
        pthread_mutex_unlock(&_buffer_mtx);
        sem_post(&_buffer_sem);
        return nullptr;
    }
    
    Buffer* buf = _free_buffers.front();
    _free_buffers.pop();
    pthread_mutex_unlock(&_buffer_mtx);

    Ethernet::Frame* frame = buf->data();
    frame->src = address();
    frame->dst = dst;
    frame->prot = prot;

    return buf;
}

template <typename Engine>
int NIC<Engine>::send(Buffer* buf) {
    if (!buf) {
        _statistics.tx_drops++;
        return -1;
    }

    int result = Engine::send(buf->data(), buf->size());
    if (result <= 0) {
        _statistics.tx_drops++;
        return -1;
    }

    _statistics.packets_sent++;
    _statistics.bytes_sent += result;
    return result;
}

template <typename Engine>
void NIC<Engine>::free(Buffer* buf) {
    if (!buf) return;

    buf->clear();

    pthread_mutex_lock(&_buffer_mtx);
    _free_buffers.push(buf);
    pthread_mutex_unlock(&_buffer_mtx);

    sem_post(&_buffer_sem);
}

template <typename Engine>
const typename NIC<Engine>::Address& NIC<Engine>::address() {
    return Ethernet::address();
}

template <typename Engine>
void NIC<Engine>::address(Address address) {
    Ethernet::address(address);
}

template <typename Engine>
const typename NIC<Engine>::Statistics& NIC<Engine>::statistics() {
    return _statistics;
}

template <typename Engine>
void NIC<Engine>::attach(Observer* obs, Protocol_Number prot) {
    Observed::attach(obs, prot);
}

template <typename Engine>
void NIC<Engine>::detach(Observer* obs, Protocol_Number prot) {
    Observed::detach(obs, prot);
}

#endif // NIC_H