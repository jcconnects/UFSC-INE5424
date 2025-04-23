#include "../../include/communicator.h"
#include "../../include/message.h"
// Required includes for the Initializer stub
#include "../../include/nic.h"
#include "../../include/protocol.h"
#include "../../include/socketEngine.h"
#include "../../include/ethernet.h"
// ---
#include "test_utils.h"
#include <pthread.h>
#include <unistd.h>
#include <atomic>
#include <iostream>
#include <chrono> // For sleep_for
#include <cstring> // For strlen

std::atomic<bool> thread_done(false);

// Protocol Stub / Initializer (remains the same)
class Initializer {
    public:
        typedef NIC<SocketEngine> NICType;
        typedef Protocol<NICType> ProtocolType;

        static NICType* create_nic(unsigned int id) {
            // Setting virtual MAC Address
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
        
        static ProtocolType* create_protocol(NICType* nic) {
            return new ProtocolType(nic);
        }
};

// Updated receive function to match new signature
void* run_recv(void* comm) {
    Communicator<Initializer::ProtocolType>* c = static_cast<Communicator<Initializer::ProtocolType>*>(comm);
    Message<1488> m = Message<1488>();
    Initializer::ProtocolType::Address source_addr; // Variable to store source address

    TEST_LOG_THREAD("Receiver thread started, waiting for message...");
    // Call receive with new signature
    bool received_ok = c->receive(&m, &source_addr);

    if (received_ok) {
        TEST_LOG_THREAD("Receiver thread received message successfully from " + source_addr.to_string());
        // TODO: Add assertion here to check message content m.data() and m.size()
        // TODO: Add assertion here to check source_addr against expected sender (e.g., comm1_address)
        thread_done.store(true);
    } else {
        TEST_LOG_THREAD("Receiver thread receive() returned false (likely timeout or close).");
        // thread_done remains false
    }
    return static_cast<void*>(nullptr);
}


int main() {
    TEST_INIT("communicator_test");

    TEST_LOG("Creating NIC and Protocol instances");
    Initializer::NICType* nic1 = Initializer::create_nic(1);
    Initializer::NICType* nic2 = Initializer::create_nic(2);
    Initializer::ProtocolType* prot1 = Initializer::create_protocol(nic1); // Protocol for comm1
    // Using a single protocol instance for both communicators for this test.
    // This assumes the underlying protocol/NIC can handle routing based on address.

    // Define addresses for the communicators
    auto comm1_address = Initializer::ProtocolType::Address(nic1->address(), 111); // Use specific ports
    auto comm2_address = Initializer::ProtocolType::Address(nic2->address(), 222);

    TEST_LOG("Creating Communicator instances with addresses: " + comm1_address.to_string() + " and " + comm2_address.to_string());
    Communicator<Initializer::ProtocolType>* comm1 = new Communicator<Initializer::ProtocolType>(prot1, comm1_address);
    Communicator<Initializer::ProtocolType>* comm2 = new Communicator<Initializer::ProtocolType>(prot1, comm2_address); // Using shared protocol prot1

    pthread_t thread_id_recv1;
    thread_done.store(false); // Reset flag

    // Test 1: Close (Adapted - check if receive returns false)
    TEST_LOG("--- Test 1: Close ---");
    TEST_LOG("Starting receiver thread (comm1) and closing communicator...");
    pthread_create(&thread_id_recv1, nullptr, run_recv, comm1);
    // Give thread a moment to start waiting in receive()
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    comm1->close(); // Close should interrupt receive()
    pthread_join(thread_id_recv1, nullptr); // Wait for thread to finish
    TEST_ASSERT(!thread_done.load(), "Receive should return false when communicator is closed");
    TEST_LOG("Test 1 Passed.");


    // Test 2 & 3: Send/Receive (Adapted)
    TEST_LOG("--- Test 2 & 3: Send/Receive ---");
    pthread_t thread_id_recv2;
    thread_done.store(false); // Reset flag

    // Start receiver thread for comm2
    TEST_LOG("Starting receiver thread (comm2)...");
    pthread_create(&thread_id_recv2, nullptr, run_recv, comm2);
    // Give thread time to initialize and wait in receive()
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Send message from comm1 to comm2
    const char* test_payload = "a message";
    size_t payload_len = strlen(test_payload) + 1; // Include null terminator
    TEST_LOG("Sending message from comm1 (" + comm1_address.to_string() + ") to comm2 (" + comm2_address.to_string() + ")");
    Message<1488> m_send(test_payload, payload_len);
    bool sent = comm1->send(comm2_address, &m_send); // Use new send signature
    TEST_ASSERT(sent, "Send should return true");

    // Wait for the receiver thread to signal completion
    TEST_LOG("Waiting for receiver thread (comm2) to finish...");
    pthread_join(thread_id_recv2, nullptr);
    TEST_ASSERT(thread_done.load(), "Receive should return true after message sent");
    // TODO: Add check for received message content if stored globally/returned by thread.
    TEST_LOG("Test 2 & 3 Passed.");


    // Cleanup
    TEST_LOG("Cleaning up...");
    // comm1 was closed in Test 1, comm2 needs closing.
    comm2->close(); // Explicitly close comm2 before deleting
    delete comm1;
    delete comm2;
    delete prot1;
    delete nic1;
    delete nic2;

    std::cout << "Communicator test passed successfully!" << std::endl;

    return 0;
}