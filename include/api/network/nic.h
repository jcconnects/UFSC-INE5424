#ifndef NIC_H
#define NIC_H

#include <semaphore.h>
#include <pthread.h>
#include <queue>
#include <atomic>
#include <time.h>
#include <cstdint>
#include <string>
#include <cstring>
#include <fstream>

#include "api/traits.h"
#include "api/network/ethernet.h"
#include "api/util/debug.h"
#include "api/util/observer.h"
#include "api/util/observed.h"
#include "api/util/buffer.h"
#include "api/framework/clock.h"  // Include Clock for timestamping

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
        
        // Send a pre-allocated buffer with packet size for timestamp offset calculation
        int send(DataBuffer* buf, unsigned int packet_size);
        // Legacy send method for backward compatibility
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

        void stop();
        
        double radius();
        void setRadius(double radius);
        // Attach/detach observers
        // void attach(Observer* obs, Protocol_Number prot); // inherited
        // void detach(Observer* obs, Protocol_Number prot); // inherited

    private:
        void handle(Ethernet::Frame* frame, unsigned int size) override;

        // Helper method to fill TX timestamp in packet
        void fillTxTimestamp(DataBuffer* buf, unsigned int packet_size);

        // Helper method to log latency to CSV file
        void logLatency(std::int64_t latency_us);

    private:

        Address _address;
        Statistics _statistics;
        DataBuffer _buffer[N_BUFFERS];
        std::queue<DataBuffer*> _free_buffers;
        std::atomic<bool> _running;
        sem_t _buffer_sem;
        sem_t _binary_sem;
        double _radius;
        std::ofstream _latency_csv_file;

        bool running() { return _running.load(std::memory_order_acquire); }
};

/*********** NIC Implementation ************/
template <typename Engine>
NIC<Engine>::NIC() : _running(true), _radius(1000.0) {  // Default 1000m transmission radius
    // Initialize buffers FIRST - before starting Engine
    db<NIC>(INF) << "[NIC] [constructor] initializing buffers and semaphores\n";
    
    for (unsigned int i = 0; i < N_BUFFERS; ++i) {
        _buffer[i] = DataBuffer();
        _free_buffers.push(&_buffer[i]);
    }
    
    sem_init(&_buffer_sem, 0, N_BUFFERS);
    
    sem_init(&_binary_sem, 0, 1);
    
    // Setting default address - must be done before starting Engine
    _address = Engine::mac_address();
    
    // Initialize CSV file for latency logging
    _latency_csv_file.open("nic_latency.csv", std::ios::out | std::ios::app);
    if (_latency_csv_file.is_open()) {
        // Check if file is empty (new file) to write header
        std::ifstream test_file("nic_latency.csv");
        test_file.seekg(0, std::ios::end);
        if (test_file.tellg() == 0) {
            _latency_csv_file << "latency_us\n";
        }
        test_file.close();
        db<NIC>(INF) << "[NIC] [constructor] CSV latency log file opened\n";
    } else {
        db<NIC>(WRN) << "[NIC] [constructor] Failed to open CSV latency log file\n";
    }
    
    // NOW it's safe to start the Engine - all NIC infrastructure is ready
    Engine::start();
    db<NIC>(INF) << "[NIC] [constructor] NIC fully initialized and Engine started with default radius " << _radius << "m\n";
}

template <typename Engine>
NIC<Engine>::~NIC() {
    // Destroy engine first
    stop();

    // Close CSV file
    if (_latency_csv_file.is_open()) {
        _latency_csv_file.close();
        db<NIC>(INF) << "[NIC] [destructor] CSV latency log file closed\n";
    }

    sem_destroy(&_buffer_sem);
    sem_destroy(&_binary_sem);
    db<NIC>(INF) << "[NIC] [destructor] semaphores destroyed\n";
    // Engine stops itself on its own
}

template <typename Engine>
void NIC<Engine>::stop() {
    db<NIC>(TRC) << "[NIC] [stop()] called!\n";
    _running.store(false, std::memory_order_release);
    int sem_value;
    sem_getvalue(&_buffer_sem, &sem_value);
    for (unsigned int i = 0; i < N_BUFFERS - sem_value; ++i) {
        sem_post(&_buffer_sem); // Release the semaphore for each buffer
    }
    Engine::stop();
}

template <typename Engine>
int NIC<Engine>::send(DataBuffer* buf, unsigned int packet_size) {
    db<NIC>(TRC) << "NIC<Engine>::send() called!\n";
    if (!running()) {
        db<NIC>(TRC) << "[NIC] send called when NIC is not running \n";
        return 0;
    }

    if (!buf) {
        db<NIC>(WRN) << "[NIC] send() called with a null buffer\n";
        _statistics.tx_drops++;
        return 0;
    }

    // Fill TX timestamp before sending
    fillTxTimestamp(buf, packet_size);

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
int NIC<Engine>::send(DataBuffer* buf) {
    db<NIC>(TRC) << "NIC<Engine>::send() called!\n";

    if (!running()) {
        db<NIC>(ERR) << "[NIC] send() called when NIC is inactive\n";
        return -1;
    }

    if (!buf) {
        db<NIC>(WRN) << "[NIC] send() called with a null buffer\n";
        _statistics.tx_drops++;
        return -1;
    }

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

    if (!running()) {
        db<NIC>(ERR) << "[NIC] receive() called when NIC is inactive\n";
        return -1;
    }

    if (!buf) {
        db<NIC>(WRN) << "[NIC] receive() called with a null buffer\n";
        _statistics.rx_drops++;
        return 0;
    }

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
    db<NIC>(TRC) << "[NIC] [handle()] called!\n";

    // Additional safety check - ensure we're fully initialized
    if (!running()) {
        db<NIC>(WRN) << "[NIC] [handle()] called but NIC is not running - ignoring packet\n";
        return;
    }
    
    // Filter check (optional, engine might do this): ignore packets sent by self
    if (frame->src == _address) {
        db<NIC>(INF) << "[NIC] [handle()] ignoring frame from self: {src=" << Ethernet::mac_to_string(frame->src) << "}\n";
        return;
   }

    // 1. Extracting frame header
    Ethernet::Address dst = frame->dst;
    Ethernet::Protocol proto = frame->prot;

    // 2. Allocate buffer
    unsigned int packet_size = size - Ethernet::HEADER_SIZE;
    if (packet_size == 0) {
        db<NIC>(INF) << "[NIC] [handle()] dropping empty frame\n";
        _statistics.rx_drops++;
        return;
    }

    db<NIC>(TRC) << "[NIC] [handle()] allocating buffer\n";
    DataBuffer * buf = alloc(dst, proto, packet_size);
    db<NIC>(TRC) << "[NIC] [handle()] buffer allocated\n";

    if (!buf) {
        db<NIC>(ERR) << "[NIC] [handle()] alloc called, but NIC is not running\n";
        return;
    }

    // 3. Fill RX timestamp in the buffer
    db<NIC>(TRC) << "[NIC] [handle()] filling RX timestamp in the buffer\n";
    auto& clock = Clock::getInstance();
    TimestampType rx_time = clock.getLocalSystemTime();
    std::int64_t timestamp = rx_time.time_since_epoch().count();
    buf->setRX(timestamp);
    db<NIC>(TRC) << "[NIC] [handle()] RX timestamp filled in the buffer: " << timestamp << "\n";

    // 4. Copy frame to buffer
    db<NIC>(TRC) << "[NIC] [handle()] copying frame to buffer\n";
    buf->setData(frame, size);
    db<NIC>(TRC) << "[NIC] [handle()] frame copied to buffer\n";

    // 5. Extract TX timestamp and calculate latency for logging
    db<NIC>(TRC) << "[NIC] [handle()] extracting TX timestamp for latency calculation\n";
    const unsigned int header_size = sizeof(std::uint16_t) * 2 + sizeof(std::uint32_t); // 8 bytes
    const unsigned int tx_timestamp_offset = header_size + 8; // Header + offsetof(TimestampFields, tx_timestamp)
    
    if (packet_size > tx_timestamp_offset + sizeof(TimestampType)) {
        const TimestampType* tx_timestamp_ptr = reinterpret_cast<const TimestampType*>(frame->payload + tx_timestamp_offset);
        TimestampType tx_time = *tx_timestamp_ptr;
        
        // Calculate latency in microseconds
        std::int64_t latency_us = (rx_time - tx_time).count();
        
        db<NIC>(INF) << "[NIC] [handle()] Latency calculated: TX=" << tx_time.time_since_epoch().count() 
                      << "us, RX=" << rx_time.time_since_epoch().count() << "us, Latency=" << latency_us << "us\n";
        
        // Log latency to CSV file
        logLatency(latency_us);
    } else {
        db<NIC>(WRN) << "[NIC] [handle()] Packet too small for TX timestamp extraction. Size: " << packet_size 
                      << ", required: " << (tx_timestamp_offset + sizeof(TimestampType)) << "\n";
    }

   if (!running()) {
        db<NIC>(ERR) << "[NIC] [handle()] trying to notify protocol when NIC is inactive\n";
        return;
    }

    // 6. Notify Observers
    if (!notify(buf, proto)) {
        db<NIC>(INF) << "[NIC] [handle()] data received, but no one was notified (" << proto << ")\n";
        free(buf); // if no one is listening, release buffer
    }
}

template <typename Engine>
typename NIC<Engine>::DataBuffer* NIC<Engine>::alloc(Address dst, Protocol_Number prot, unsigned int size) {
    db<NIC>(TRC) << "[NIC] [alloc()] called!\n";

    if (!running()) {
        db<NIC>(ERR) << "[NIC] [alloc()] called when NIC is inactive\n";
        return nullptr;
    }
    
    // Acquire free buffers counter semaphore
    db<NIC>(TRC) << "[NIC] [alloc()] acquiring free buffers counter semaphore\n";
    sem_wait(&_buffer_sem);
    db<NIC>(TRC) << "[NIC] [alloc()] free buffers counter semaphore acquired\n";

    // Remove first buffer of the free buffers queue
    db<NIC>(TRC) << "[NIC] [alloc()] acquiring binary semaphore\n";
    sem_wait(&_binary_sem);
    db<NIC>(TRC) << "[NIC] [alloc()] binary semaphore acquired\n";
    DataBuffer* buf = _free_buffers.front();
    _free_buffers.pop();
    db<NIC>(TRC) << "[NIC] [alloc()] buffer removed from free buffers queue\n";
    sem_post(&_binary_sem);
    db<NIC>(TRC) << "[NIC] [alloc()] binary semaphore released\n";

    // Set Frame
    Ethernet::Frame frame;
    frame.src = address();
    frame.dst = dst;
    frame.prot = prot;
    unsigned int frame_size = size + Ethernet::HEADER_SIZE;

    // Set buffer data
    buf->setData(&frame, frame_size);

    db<NIC>(INF) << "[NIC] [alloc()] buffer allocated for frame: {src = " << Ethernet::mac_to_string(frame.src) << ", dst = " << Ethernet::mac_to_string(frame.dst) << ", prot = " << std::to_string(frame.prot) << ", size = " << buf->size() << "}\n";

    return buf;
}

template <typename Engine>
void NIC<Engine>::free(DataBuffer* buf) {
    db<NIC>(TRC) << "NIC<Engine>::free() called!\n";

    if (buf == nullptr) {
        db<NIC>(WRN) << "[NIC] free() called with a null buffer\n";
        return;
    }

    if (!running()) {
        db<NIC>(ERR) << "[NIC] free() called when NIC is inactive\n";
        return;
    }

    // Debug: Check semaphore state before freeing
    int sem_value;
    sem_getvalue(&_buffer_sem, &sem_value);
    db<NIC>(INF) << "[NIC] [free()] freeing buffer, current semaphore value: " << sem_value << "\n";

    // Clear buffer
    buf->clear();
    
    // Add to free buffers queue
    sem_wait(&_binary_sem);
    _free_buffers.push(buf);
    size_t queue_size = _free_buffers.size();
    sem_post(&_binary_sem);
    
    // Increase free buffers counter
    sem_post(&_buffer_sem);
    
    // Debug: Check semaphore state after freeing
    sem_getvalue(&_buffer_sem, &sem_value);
    db<NIC>(INF) << "[NIC] buffer released, new semaphore value: " << sem_value << ", queue size: " << queue_size << "\n";
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

template <typename Engine>
void NIC<Engine>::fillTxTimestamp(DataBuffer* buf, unsigned int packet_size) {
    db<NIC>(TRC) << "NIC<Engine>::fillTxTimestamp() called!\n";
    
    // Get current synchronized time from Clock
    auto& clock = Clock::getInstance();
    bool is_sync = true;
    TimestampType tx_time = clock.getLocalSystemTime();
    
    // Calculate correct offset for TX timestamp accounting for structure alignment
    // Header: 8 bytes (2×uint16_t + uint32_t)
    // TimestampFields: bool at offset 0, tx_timestamp at offset 8 (due to alignment)
    const unsigned int header_size = sizeof(std::uint16_t) * 2 + sizeof(std::uint32_t); // 8 bytes
    const unsigned int tx_timestamp_offset = header_size + 8; // Header + offsetof(TimestampFields, tx_timestamp)
    
    // Get pointer to the packet within the Ethernet frame payload
    Ethernet::Frame* frame = buf->data();
    uint8_t* packet_start = frame->payload;
    
    // Fill TX timestamp at the calculated offset
    if (packet_size > tx_timestamp_offset + sizeof(TimestampType)) {
        TimestampType* tx_timestamp_ptr = reinterpret_cast<TimestampType*>(packet_start + tx_timestamp_offset);
        *tx_timestamp_ptr = tx_time;
        
        db<NIC>(INF) << "[NIC] Filled TX timestamp at offset " << tx_timestamp_offset 
                      << ": " << tx_time.time_since_epoch().count() << "us\n";
    } else {
        db<NIC>(WRN) << "[NIC] Packet too small for TX timestamp. Size: " << packet_size 
                      << ", required: " << (tx_timestamp_offset + sizeof(TimestampType)) << "\n";
    }
}

template <typename Engine>
double NIC<Engine>::radius() {
    return _radius;
}

template <typename Engine>
void NIC<Engine>::setRadius(double radius) {
    _radius = radius;
}

/**
 * @brief Log latency measurement to CSV file
 * 
 * @param latency_us The latency in microseconds to log
 */
template <typename Engine>
void NIC<Engine>::logLatency(std::int64_t latency_us) {
    if (_latency_csv_file.is_open()) {
        _latency_csv_file << latency_us << "\n";
        _latency_csv_file.flush(); // Ensure data is written immediately
        db<NIC>(TRC) << "[NIC] [logLatency] Logged latency: " << latency_us << " us\n";
    } else {
        db<NIC>(WRN) << "[NIC] [logLatency] CSV file not open, cannot log latency\n";
    }
}

#endif // NIC_H