#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "observer.h"
#include "nic.h"
#include "traits.h"
#include "ethernet.h"

// Define XXX as a placeholder for Port type
typedef unsigned short XXX;

// Communication Protocol
template <typename NIC>
class Protocol: private typename NIC::Observer
{
    public:
        static const typename NIC::Protocol_Number PROTO = Traits<Protocol>::ETHERNET_PROTOCOL_NUMBER;
        typedef typename NIC::Buffer Buffer;
        typedef typename NIC::Address Physical_Address;
        typedef XXX Port;
        typedef Conditional_Data_Observer<Buffer<Ethernet::Frame>, Port> Observer;
        typedef Conditionally_Data_Observed<Buffer<Ethernet::Frame>, Port> Observed;
        
        class Address
        {
            public:
                enum Null { NULL_VALUE };
            public:
                Address();
                Address(const Null & null);
                Address(Physical_Address paddr, Port port);
                operator bool() const { return (_paddr || _port); }
                bool operator==(Address a) { return (_paddr == a._paddr) && (_port == a._port); }
            private:
                Physical_Address _paddr;
                Port _port;
        };
        
        class Header;
        static const unsigned int MTU = NIC::MTU - sizeof(Header);
        typedef unsigned char Data[MTU];
        
        class Packet: public Header
        {
            public:
                Packet();

                Header * header();

                template<typename T>
                T * data() { return reinterpret_cast<T *>(&_data); }
            private:
                Data _data;
        } __attribute__((packed));
        
    protected:
        Protocol(NIC * nic): _nic(nic) { _nic->attach(this, PROTO); }
        
    public:
        ~Protocol() { _nic->detach(this, PROTO); }
        
        static int send(Address from, Address to, const void * data, unsigned int size);
        // Buffer * buf = NIC::alloc(to.paddr, PROTO, sizeof(Header) + size)
        // NIC::send(buf)
        
        static int receive(Buffer * buf, Address from, void * data, unsigned int size);
        // unsigned int s = NIC::receive(buf, &from.paddr, &to.paddr, data, size)
        // NIC::free(buf)
        // return s;
        
        static void attach(Observer * obs, Address address);
        static void detach(Observer * obs, Address address);
        
    private:
        void update(typename NIC::Observed * obs, NIC::Protocol_Number prot, Buffer * buf) {
            if(!_observed.notify(buf)) // to call receive(...);
                _nic->free(buf);
        }
        
    private:
        NIC * _nic;
        // Channel protocols are usually singletons
        static Observed _observed;
};

#endif // PROTOCOL_H
