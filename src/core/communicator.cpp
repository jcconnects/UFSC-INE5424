#include "communicator.h"
#include <iostream>

// Basic Message implementation for testing
class SimpleMessage : public Message {
public:
    SimpleMessage(const void* data, unsigned int size) : _size(size) {
        _data = new char[size];
        if (data) {
            memcpy(_data, data, size);
        } else {
            memset(_data, 0, size);
        }
    }
    
    ~SimpleMessage() {
        delete[] _data;
    }
    
    const void* data() const override {
        return _data;
    }
    
    unsigned int size() const override {
        return _size;
    }
    
private:
    char* _data;
    unsigned int _size;
};

// Example Channel for instantiation
class DummyChannel {
public:
    class Observer {
    public:
        typedef int Observed_Data;
        typedef int Observing_Condition;
    };
    
    class Observed {
    public:
        Observed() {}
    };
    
    typedef int Buffer;
    
    class Address {
    public:
        static const Address BROADCAST;
        
        Address() {}
        
        bool operator==(const Address& other) const {
            return true; // Placeholder implementation
        }
    };
    
    void attach(void* obs, Address addr) {
        std::cout << "DummyChannel attaching observer" << std::endl;
    }
    
    static void detach(void* obs, Address addr) {
        std::cout << "DummyChannel detaching observer" << std::endl;
    }
    
    int send(Address from, Address to, const void* data, unsigned int size) {
        std::cout << "DummyChannel sending data of size " << size << std::endl;
        return size;
    }
    
    int receive(Buffer* buf, Address* from, void* data, unsigned int size) {
        std::cout << "DummyChannel receiving data, max size " << size << std::endl;
        return size;
    }
};

// Initialize static member
const DummyChannel::Address DummyChannel::Address::BROADCAST;

// Explicit template instantiation for common Channel types
template class Communicator<DummyChannel>;
