#ifndef NIC_H
#define NIC_H

#include "observed.h"
#include "stubs/socketengine.h"
#include <string>
#include <iostream>
#include <atomic>

// Network Interface Card implementation
template <typename Engine>
class NIC: public Conditionally_Data_Observed<BufferStub, int>
{
public:
    typedef std::string Address;
    typedef int Protocol_Number;
    typedef BufferStub Buffer;
    typedef Conditional_Data_Observer<Buffer, Protocol_Number> Observer;
    typedef Conditionally_Data_Observed<Buffer, Protocol_Number> Observed;
    
    // Statistics for network operations
    struct Statistics {
        std::atomic<unsigned int> packets_sent;
        std::atomic<unsigned int> packets_received;
        std::atomic<unsigned int> bytes_sent;
        std::atomic<unsigned int> bytes_received;
        
        Statistics() : packets_sent(0), packets_received(0), bytes_sent(0), bytes_received(0) {}
    };

    NIC() : _engine() {
        std::cout << "NIC created" << std::endl;
    }
    
    ~NIC() {
        std::cout << "NIC destroyed" << std::endl;
    }
    
    // Send data over the network
    int send(Address dst, Protocol_Number prot, const void* data, unsigned int size) {
        std::cout << "NIC sending to " << dst << " on protocol " << prot << std::endl;
        int result = _engine.send(data, size);
        
        if (result > 0) {
            _statistics.packets_sent++;
            _statistics.bytes_sent += result;
        }
        
        return result;
    }
    
    // Receive data from the network
    int receive(Address* src, Protocol_Number* prot, void* data, unsigned int size) {
        std::cout << "NIC receiving up to " << size << " bytes" << std::endl;
        int result = _engine.receive(data, size);
        
        if (result > 0) {
            if (src) *src = "remote_address"; // Simulate source address
            if (prot) *prot = 0; // Simulate protocol number
            
            _statistics.packets_received++;
            _statistics.bytes_received += result;
        }
        
        return result;
    }
    
    // Process a received buffer
    int receive(Buffer* buf, Address* src, Address* dst, void* data, unsigned int size) {
        std::cout << "NIC processing buffer" << std::endl;
        
        if (!buf) return 0;
        
        // Copy buffer data to the output buffer
        size_t copy_size = std::min<size_t>(size, buf->data.size());
        memcpy(data, buf->data.c_str(), copy_size);
        
        // Set source and destination addresses
        if (src) *src = "remote_address";
        if (dst) *dst = _address;
        
        return copy_size;
    }
    
    // Allocate a buffer for sending
    Buffer* alloc(Address dst, Protocol_Number prot, unsigned int size) {
        return new Buffer(std::string(size, '\0'));
    }
    
    // Send a pre-allocated buffer
    int send(Buffer* buf) {
        if (!buf) return 0;
        
        int result = _engine.send(buf->data.c_str(), buf->data.size());
        
        if (result > 0) {
            _statistics.packets_sent++;
            _statistics.bytes_sent += result;
        }
        
        return result;
    }
    
    // Free a buffer after use
    void free(Buffer* buf) {
        delete buf;
    }
    
    // Get the local address
    const Address& address() {
        _address = _engine.getLocalAddress();
        return _address;
    }
    
    // Set the local address
    void address(Address address) {
        _address = address;
    }
    
    // Get network statistics
    const Statistics& statistics() {
        return _statistics;
    }

private:
    Engine _engine;
    Address _address;
    Statistics _statistics;
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
