#include "../../include/communicator.h"
#include "../../include/message.h"
#include "test_utils.h"
#include <pthread.h>
#include <unistd.h>
#include <atomic>
#include <iostream>

std::atomic<bool> thread_done(false);

// Protocol Stub
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

void* run_recv(void* comm) {
    Communicator<Initializer::ProtocolType>* c = static_cast<Communicator<Initializer::ProtocolType>*>(comm);
    Message<1488> m = Message<1488>();
    c->receive(&m);

    thread_done.store(true);
    return static_cast<void*>(nullptr);
}

int main() {
    TEST_INIT("communicator_test");

    TEST_LOG("Creating Communicator instance");
    // Create NIC instances
    Initializer::NICType* nic1 = Initializer::create_nic(1);
    Initializer::NICType* nic2 = Initializer::create_nic(2);
    // Create Protocol instances
    Initializer::ProtocolType* prot = Initializer::create_protocol(nic1);

    // Create Communicator instances
    Communicator<Initializer::ProtocolType>* comm1 = new Communicator<Initializer::ProtocolType>(prot, Initializer::ProtocolType::Address(nic1->address(), Initializer::ProtocolType::Address::NULL_VALUE));

    Communicator<Initializer::ProtocolType>* comm2 = new Communicator<Initializer::ProtocolType>(prot, Initializer::ProtocolType::Address(nic2->address(), Initializer::ProtocolType::Address::NULL_VALUE));

    pthread_t thread_id;
    // Test 1: Close
    // Calls receive with no message to recover, close() call causes receive function to return
    pthread_create(&thread_id, nullptr, run_recv, &comm1);
    comm1->close();
    TEST_ASSERT(!thread_done.load(), "Return value 'receive_successfull' should be false");

    pthread_t thread_id2;
    pthread_create(&thread_id2, nullptr, run_recv, &comm2);
    
    // Test 2: Send
    // Create a Message instance
    Message<1488> m2("a message", 9);
    // Send the message
    bool sent = comm1->send(&m2);
    TEST_ASSERT(sent, "Return value 'sent' should be true");
    pthread_join(thread_id2, nullptr);

    // Test 3: Receive
    // Create a thread to run close in two seconds in case receive does not return until then
    pthread_t thread_id3;
    thread_done.store(false);
    pthread_create(&thread_id3, nullptr, run_recv, &comm2);
    sleep(1);
    TEST_ASSERT(thread_done.load(), "Return value 'receive_successfull' should be true");
    comm2->close();


    pthread_join(thread_id3, nullptr);
    pthread_join(thread_id2, nullptr);


    // Deleting communicators
    delete comm1;
    delete comm2;
    delete prot;
    delete nic1;
    delete nic2;

    std::cout << "Communicator test passed successfully!" << std::endl;

    return 0;
}