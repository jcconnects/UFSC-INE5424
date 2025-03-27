#include "../include/observer.h"
#include "../include/observed.h"
#include "../include/stubs/communicator_stubs.h"
#include "../include/communicator.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <cassert>
#include <atomic>
#include <mutex>

// Thread-safe output helper
class ThreadSafeOutput {
public:
    static void print(const std::string& msg) {
        std::lock_guard<std::mutex> lock(_mutex);
        std::cout << msg << std::endl;
    }
    
private:
    static std::mutex _mutex;
};

std::mutex ThreadSafeOutput::_mutex;

// Test class to encapsulate all tests
class CommunicatorTester {
public:
    void runAllTests() {
        ThreadSafeOutput::print("\n--- Running Communicator Tests ---\n");
        
        testCreationAndDestruction();
        testSendMessage();
        testReceiveMessage();
        testConcurrentCommunication();
        
        ThreadSafeOutput::print("\n--- All Communicator Tests Passed ---\n");
    }

private:
    // Test basic creation and destruction
    void testCreationAndDestruction() {
        ThreadSafeOutput::print("Testing Communicator creation and destruction...");
        
        ProtocolStub protocol;
        ProtocolStub::Address address("test_address", 1234);
        
        {
            // Create a communicator and let it go out of scope
            Communicator<ProtocolStub> communicator(&protocol, address);
            ThreadSafeOutput::print("Communicator created successfully");
        }
        
        ThreadSafeOutput::print("Communicator destroyed successfully");
    }
    
    // Test sending a message
    void testSendMessage() {
        ThreadSafeOutput::print("\nTesting sending messages...");
        
        ProtocolStub protocol;
        ProtocolStub::Address address("test_address", 1234);
        Communicator<ProtocolStub> communicator(&protocol, address);
        
        // Create and send a test message
        Message message("Test message 1");
        bool result = communicator.send(&message);
        
        assert(result && "Send should succeed");
        assert(protocol.hasMessage("Test message 1") && "Protocol should have the message");
        assert(protocol.getSentCount() == 1 && "There should be exactly one message sent");
        
        // Send another message
        Message message2("Test message 2");
        result = communicator.send(&message2);
        
        assert(result && "Second send should succeed");
        assert(protocol.hasMessage("Test message 2") && "Protocol should have the second message");
        assert(protocol.getSentCount() == 2 && "There should be exactly two messages sent");
        
        ThreadSafeOutput::print("Send message test passed successfully");
    }
    
    // Test receiving a message
    void testReceiveMessage() {
        ThreadSafeOutput::print("\nTesting receiving messages...");
        
        ProtocolStub protocol;
        ProtocolStub::Address address("test_address", 5678);
        
        // Start a thread that will create a communicator and receive
        std::thread receiver_thread([&protocol, &address]() {
            ThreadSafeOutput::print("Receiver thread starting...");
            
            Communicator<ProtocolStub> communicator(&protocol, address);
            
            // Try to receive a message (this will block until a message is available)
            Message received_msg;
            bool result = communicator.receive(&received_msg);
            
            assert(result && "Receive should succeed");
            ThreadSafeOutput::print("Received message: " + received_msg.getContent());
            assert(received_msg.getContent() == "Hello receiver!" && "Received message content should match");
            
            ThreadSafeOutput::print("Receiver thread completed successfully");
        });
        
        // Give the receiver thread time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Simulate receiving a message from the network
        ThreadSafeOutput::print("Simulating message reception from the network...");
        protocol.simulateReceive("Hello receiver!", 5678);
        
        // Wait for the receiver thread to complete
        receiver_thread.join();
        
        ThreadSafeOutput::print("Receive message test passed successfully");
    }
    
    // Test concurrent communication
    void testConcurrentCommunication() {
        ThreadSafeOutput::print("\nTesting concurrent communication...");
        
        ProtocolStub protocol;
        
        // Create multiple communicators on different ports
        const int NUM_COMMUNICATORS = 5;
        std::vector<std::unique_ptr<Communicator<ProtocolStub>>> communicators;
        
        for (int i = 0; i < NUM_COMMUNICATORS; i++) {
            ProtocolStub::Address addr("addr_" + std::to_string(i), 2000 + i);
            communicators.push_back(std::make_unique<Communicator<ProtocolStub>>(&protocol, addr));
        }
        
        // Start receiver threads
        std::vector<std::thread> receiver_threads;
        std::atomic<int> messages_received(0);
        
        for (int i = 0; i < NUM_COMMUNICATORS; i++) {
            receiver_threads.emplace_back([i, &communicators, &messages_received]() {
                for (int j = 0; j < 3; j++) {
                    Message received_msg;
                    bool result = communicators[i]->receive(&received_msg);
                    
                    if (result) {
                        ThreadSafeOutput::print("Communicator " + std::to_string(i) + 
                                               " received: " + received_msg.getContent());
                        messages_received++;
                    }
                }
            });
        }
        
        // Start sender threads
        std::vector<std::thread> sender_threads;
        std::atomic<int> messages_sent(0);
        
        for (int i = 0; i < NUM_COMMUNICATORS; i++) {
            sender_threads.emplace_back([i, &communicators, &messages_sent]() {
                for (int j = 0; j < 3; j++) {
                    std::string msg_content = "Message from " + std::to_string(i) + 
                                             " to all, number " + std::to_string(j);
                    Message msg(msg_content);
                    
                    bool result = communicators[i]->send(&msg);
                    if (result) {
                        messages_sent++;
                    }
                    
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
            });
        }
        
        // Simulate receiving messages for each communicator
        for (int i = 0; i < NUM_COMMUNICATORS; i++) {
            for (int j = 0; j < 3; j++) {
                std::string msg_content = "Network message to " + std::to_string(i) + 
                                         ", number " + std::to_string(j);
                                         
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                protocol.simulateReceive(msg_content, 2000 + i);
            }
        }
        
        // Wait for all threads to complete
        for (auto& t : sender_threads) {
            t.join();
        }
        
        for (auto& t : receiver_threads) {
            t.join();
        }
        
        // Verify results
        ThreadSafeOutput::print("Messages sent: " + std::to_string(messages_sent.load()));
        ThreadSafeOutput::print("Messages received: " + std::to_string(messages_received.load()));
        
        assert(messages_sent.load() == NUM_COMMUNICATORS * 3 && 
               "All messages should be sent successfully");
        assert(messages_received.load() == NUM_COMMUNICATORS * 3 && 
               "All messages should be received successfully");
        
        ThreadSafeOutput::print("Concurrent communication test passed successfully");
    }
};

int main() {
    try {
        CommunicatorTester tester;
        tester.runAllTests();
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Test failed with error: " << e.what() << std::endl;
        return 1;
    }
}
