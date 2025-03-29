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
        
        Statistics();
    };

    NIC();
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

private:
    Engine _engine;
    Address _address;
    Statistics _statistics;
};

// Statistics implementation
template <typename Engine>
NIC<Engine>::Statistics::Statistics() 
    : packets_sent(0), packets_received(0), bytes_sent(0), bytes_received(0) {}

// NIC implementations
template <typename Engine>
NIC<Engine>::NIC() : _engine() {
    std::cout << "NIC created" << std::endl;
}

template <typename Engine>
NIC<Engine>::~NIC() {
    std::cout << "NIC destroyed" << std::endl;
}

template <typename Engine>
int NIC<Engine>::send(Address dst, Protocol_Number prot, const void* data, unsigned int size) {
    std::cout << "NIC sending to " << dst << " on protocol " << prot << std::endl;
    int result = _engine.send(data, size);
    
    if (result > 0) {
        _statistics.packets_sent++;
        _statistics.bytes_sent += result;
    }
    
    return result;
}

template <typename Engine>
int NIC<Engine>::receive(Address* src, Protocol_Number* prot, void* data, unsigned int size) {
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

template <typename Engine>
int NIC<Engine>::receive(Buffer* buf, Address* src, Address* dst, void* data, unsigned int size) {
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

template <typename Engine>
typename NIC<Engine>::Buffer* NIC<Engine>::alloc(Address dst, Protocol_Number prot, unsigned int size) {
    return new Buffer(std::string(size, '\0'));
}

template <typename Engine>
int NIC<Engine>::send(Buffer* buf) {
    if (!buf) return 0;
    
    int result = _engine.send(buf->data.c_str(), buf->data.size());
    
    if (result > 0) {
        _statistics.packets_sent++;
        _statistics.bytes_sent += result;
    }
    
    return result;
}

template <typename Engine>
void NIC<Engine>::free(Buffer* buf) {
    delete buf;
}

template <typename Engine>
const typename NIC<Engine>::Address& NIC<Engine>::address() {
    _address = _engine.getLocalAddress();
    return _address;
}

template <typename Engine>
void NIC<Engine>::address(Address address) {
    _address = address;
}

template <typename Engine>
const typename NIC<Engine>::Statistics& NIC<Engine>::statistics() {
    return _statistics;
}

#endif // NIC_H