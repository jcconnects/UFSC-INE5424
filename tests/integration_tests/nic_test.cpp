#define TEST_MODE 1

#include <iostream>
#include <string>
#include <cassert>
#include <cstring>
#include <thread>
#include <chrono>
#include <vector>
#include "../../include/nic.h"
#include "../../include/socketEngine.h"
#include "../../include/sharedMemoryEngine.h"
#include "../../include/ethernet.h"
#include "../../include/traits.h"
#include "test_utils.h"

class Initializer {
    public:
        typedef NIC<SocketEngine, SharedMemoryEngine> NICType;

        Initializer() = default;

        ~Initializer() = default;

        // Start the vehicle process
        static NICType* create_nic(unsigned int id);
};

/********** Initializer Implementation ***********/
Initializer::NICType* Initializer::create_nic(unsigned int id) {
    // Setting Vehicle virtual MAC Address
    Ethernet::Address addr;
    addr.bytes[0] = 0x02; // local, unicast
    addr.bytes[1] = 0x00;
    addr.bytes[2] = 0x00;
    addr.bytes[3] = 0x00;
    addr.bytes[4] = (id >> 8) & 0xFF;
    addr.bytes[5] = id & 0xFF;

    NICType* nic = new NICType();
    nic->setAddress(addr);
    return nic;
}

// Helper struct to hold statistics values (non-atomic)
struct StatsSnapshot {
    unsigned int packets_sent;
    unsigned int packets_received;
    unsigned int bytes_sent;
    unsigned int bytes_received;
    unsigned int tx_drops;
    unsigned int rx_drops;
};

// Helper function to get stats snapshot
StatsSnapshot getStats(NIC<SocketEngine, SharedMemoryEngine>* nic) {
    const auto& stats = nic->statistics();
    StatsSnapshot snapshot;
    // Direct access instead of using statistics() method and atomic loads
    snapshot.packets_sent = stats.packets_sent.load();
    snapshot.packets_received = stats.packets_received.load();
    snapshot.bytes_sent = stats.bytes_sent.load();
    snapshot.bytes_received = stats.bytes_received.load();
    snapshot.tx_drops = stats.tx_drops.load();
    snapshot.rx_drops = stats.rx_drops.load();
    return snapshot;
}

int main() {
    TEST_INIT("nic_test");
    
    TEST_LOG("Creating NIC instance");
    
    // Use the actual NIC with SocketEngine and MockInternalEngine
    typedef NIC<SocketEngine, SharedMemoryEngine> NIC_Engine;
    NIC_Engine* nic = Initializer::create_nic(1); // Use factory method with ID 1
    
    // Test 1: Address functions
    TEST_LOG("Testing address functions");
    
    // Get default address
    auto defaultAddr = nic->address();
    TEST_LOG("Default address: " + Ethernet::mac_to_string(defaultAddr));
    TEST_ASSERT(defaultAddr != Ethernet::NULL_ADDRESS, "Default address should not be null");
    
    // Set new address
    Ethernet::Address testAddr = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    nic->setAddress(testAddr);
    TEST_LOG("Set address to: " + Ethernet::mac_to_string(testAddr));
    
    // Verify address was set correctly
    auto currentAddr = nic->address();
    TEST_LOG("Current address: " + Ethernet::mac_to_string(currentAddr));
    TEST_ASSERT(memcmp(currentAddr.bytes, testAddr.bytes, 6) == 0, "Address should be updated to match the set address");
    
    // Test 2: Buffer allocation and management
    TEST_LOG("Testing buffer allocation and freeing");
    
    // Allocate a buffer
    Ethernet::Address dstAddr = {0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB};
    Ethernet::Protocol prot = 0x0800; // IPv4 protocol number
    unsigned int size = 100;
    
    TEST_LOG("Allocating buffer for frame");
    auto buf = nic->alloc(dstAddr, prot, size);
    TEST_ASSERT(buf != nullptr, "Buffer allocation should succeed");
    
    // Verify buffer properties
    Ethernet::Frame* frame = buf->data();
    TEST_ASSERT(memcmp(frame->src.bytes, nic->address().bytes, 6) == 0, "Source address should match NIC address");
    TEST_ASSERT(memcmp(frame->dst.bytes, dstAddr.bytes, 6) == 0, "Destination address should match provided address");
    TEST_ASSERT(frame->prot == prot, "Protocol should match provided protocol");
    
    // Update buffer size assertion to account for Ethernet header
    unsigned int expected_size = size + Ethernet::HEADER_SIZE;
    TEST_LOG("Buffer requested size: " + std::to_string(size) + ", actual size: " + std::to_string(buf->size()) + 
             ", header size: " + std::to_string(Ethernet::HEADER_SIZE));
    TEST_ASSERT(buf->size() == expected_size, "Buffer size should match requested size plus header size");
    
    // Test free buffer
    TEST_LOG("Freeing buffer");
    nic->free(buf);
    
    // Allocate multiple buffers to ensure reuse works
    std::vector<NIC_Engine::DataBuffer*> buffers;
    TEST_LOG("Allocating multiple buffers");
    for (int i = 0; i < 5; i++) {
        auto b = nic->alloc(dstAddr, prot, size);
        TEST_ASSERT(b != nullptr, "Buffer allocation should succeed");
        buffers.push_back(b);
    }
    
    // Free all buffers
    TEST_LOG("Freeing all buffers");
    for (auto b : buffers) {
        nic->free(b);
    }
    
    // Test 3: Statistics tracking
    TEST_LOG("Testing statistics tracking");
    
    // Get initial statistics
    StatsSnapshot initialStats = getStats(nic);
    TEST_LOG("Initial statistics: packets_sent=" + std::to_string(initialStats.packets_sent) + 
        ", packets_received=" + std::to_string(initialStats.packets_received) +
        ", bytes_sent=" + std::to_string(initialStats.bytes_sent) + 
        ", bytes_received=" + std::to_string(initialStats.bytes_received) +
        ", tx_drops=" + std::to_string(initialStats.tx_drops) + 
        ", rx_drops=" + std::to_string(initialStats.rx_drops));
    
    // All statistics should start at zero
    TEST_ASSERT(initialStats.packets_sent == 0, "Initial packets_sent should be 0");
    TEST_ASSERT(initialStats.packets_received == 0, "Initial packets_received should be 0");
    TEST_ASSERT(initialStats.bytes_sent == 0, "Initial bytes_sent should be 0");
    TEST_ASSERT(initialStats.bytes_received == 0, "Initial bytes_received should be 0");
    TEST_ASSERT(initialStats.tx_drops == 0, "Initial tx_drops should be 0");
    TEST_ASSERT(initialStats.rx_drops == 0, "Initial rx_drops should be 0");
    
    // Test error condition - should increment tx_drops
    TEST_LOG("Testing tx_drops increment with null buffer");
    int result = nic->send(nullptr);
    TEST_ASSERT(result == -1, "Send with null buffer should return -1");
    
    // Verify tx_drops was incremented
    StatsSnapshot updatedStats = getStats(nic);
    TEST_LOG("Statistics after null send: tx_drops=" + std::to_string(updatedStats.tx_drops));
    TEST_ASSERT(updatedStats.tx_drops > 0, "tx_drops should be incremented after failed send");
    
    // Test running status
    TEST_LOG("Testing running status");
    TEST_ASSERT(nic->running() == true, "NIC should be running after initialization");
    
    // Clean up - explicitly stop the NIC before deleting
    TEST_LOG("Stopping NIC instance");
    nic->stop();
    
    // Delete the NIC instance
    TEST_LOG("Cleaning up NIC instance");
    delete nic;
    
    std::cout << "NIC test passed successfully!" << std::endl;
    return 0;
}
