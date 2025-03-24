#ifndef NIC_H
#define NIC_H

#include "observer.h"
#include "ethernet.h"
#include "traits.h"
#include "buffer.h"

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
        
        Buffer * alloc(Address dst, Protocol_Number prot, unsigned int size);
        void free(Buffer * buf);
        
        int send(Buffer * buf);
        int receive(Buffer * buf, Address * src, Address * dst, void * data, unsigned int size);
        
        const Address & address();
        void address(Address address);
        
        const Statistics & statistics();
        
        void attach(Observer * obs, Protocol_Number prot); // possibly inherited
        void detach(Observer * obs, Protocol_Number prot); // possibly inherited
    
    private:
        Statistics _statistics;
        Buffer _buffer[BUFFER_SIZE];
};

#endif // NIC_H
