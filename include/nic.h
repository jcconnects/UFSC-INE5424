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

// Statistics class for network metrics
class Statistics {
public:
    Statistics() : 
        tx_packets(0), tx_bytes(0), 
        rx_packets(0), rx_bytes(0), 
        tx_drops(0), rx_drops(0) {}
    
    unsigned long tx_packets;
    unsigned long tx_bytes;
    unsigned long rx_packets;
    unsigned long rx_bytes;
    unsigned long tx_drops;
    unsigned long rx_drops;
};

// Network
template <typename Engine>
class NIC: public Ethernet, public Conditionally_Data_Observed<Buffer<Ethernet::Frame>,
Ethernet::Protocol>, private Engine
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
    
    protected:
        NIC();

    public:
        ~NIC();
        
        // Will be used on P2
        // int send(Address dst, Protocol_Number prot, const void * data, unsigned int size);
        // int receive(Address * src, Protocol_Number * prot, void * data, unsigned int size);
        
        Buffer * alloc(Address dst, Protocol_Number prot, std::size_t size);
        void free(Buffer * buf);
        
        int send(Buffer * buf);
        int receive(Buffer * buf, Address * src, Address * dst, void * data, std::size_t size);
        void receive(const void* data, std::size_t size) overrride; // inherited from SocketEngine 

        // const Address & address(); // inherited from Ethernet
        // void address(Address address); // inherited from Ethernet
        
        const Statistics & statistics();
        
        // void attach(Observer * obs, Protocol_Number prot); // inherited
        // void detach(Observer * obs, Protocol_Number prot); // inherited
    
    private:
        Statistics _statistics;
        Buffer _buffer[BUFFER_SIZE];
        std::queue<Buffer*> _free_buffers;
        sem_t _buffer_sem;
        pthread_mutex_t _buffer_mtx;
};


/************** IMPLEMENTATION ****************/
template <typename Engine>
NIC<Engine>::NIC() {
    for (unsigned int i = 0; i < BUFFER_SIZE; ++i){
        _buffer[i] = Buffer();
        _free_buffers.push(&_buffer[i]);
    }

    sem_init(&_buffer_sem, 0, BUFFER_SIZE);
    pthread_mutex_init(&_buffer_mtx, nullptr);
}

template <typename Engine>
NIC<Engine>::~NIC() {
    for (unsigned int i = 0; i < BUFFER_SIZE; ++i) {
        free(_buffer[i]);
    }

    sem_destroy(&_buffer_sem);
    pthread_mutex_destroy(&_buffer_mtx);
}

template <typename Engine>
NIC<Engine>::Buffer * NIC<Engine>::alloc(Address dst, Protocol_Number prot, std::size_t size) {
    sem_wait(&_buffer_sem);

    pthread_mutex_lock(&_buffer_mtx);
    Buffer* buf = _free_buffers.front();
    _free_buffers.pop()
    pthread_mutex_unlock(&_buffer_mtx);

    Ethernet::Frame* frame = buf->data();
    frame->src = address();
    frame->dst = dst;
    frame->prot = prot;

    buf-> setSize(frame->size(size));
    return buf;
}

template <typename Engine>
void NIC<Engine>::free(Buffer* buf) {
    buf->clear();

    pthread_mutex_lock(&_buffer_mtx);
    _free_buffers.push(buf);
    pthread_mutex_unlock(&_buffer_mtx);

    sem_post(&_buffer_sem);
}

template <typename Engine>
int NIC<Engine>::send(Buffer* buf) {
    return Engine::send(reinterpret_cast<const void*>(buf->data()), buf->size());
}

template <typename Engine>
int NIC<Engine>::receive(Buffer* buf, Address* src, Address* dst, void* data, std::size_t size) {
    Observed::notify(buf->data()->prot, buf);
    // Not sure about that
}

template <typename Engine>
void NIC<Engine>::receive(const void* data, std::size_t size) {
    const Ethernet::Frame* frame = reinterpret_cast<const Ethernet::Frame*>(data);
    Buffer* buf = alloc(frame->dst, frame->prot, size);
    buf->setData(data, size);
    receive(buf, frame->src, frame->dst, data, size);
    // Also not sure about that
}

template <typename Engine>
const Statistics& NIC<Engine>::statistics() {
    return _statistics;
}

#endif // NIC_H
