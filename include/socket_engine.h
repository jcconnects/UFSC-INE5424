#ifndef ENGINE_h
#define ENGINE_h

#include <arpa/inet.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include<linux/if_packet.h>
#include<net/ethernet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <functional>
#include <stdexcept>
#include <cstring>

class SocketEngine {
public:
    struct EthFrame {
        uint8_t dest_mac[6];
        uint8_t src_mac[6];
        uint16_t eth_type;
        uint8_t payload[];
    } __attribute__((packed));
    using callbackMethod = std::function<void(const void*, std::size_t)>;

    SocketEngine(const char* if_name);
    ~SocketEngine();

    int send(const void* data, std::size_t length);
    void setCallback(callbackMethod cb);

private:
    int _socket;
    callbackMethod _cb;
    unsigned int ifindex;

    void asyncReceive();
    static void signalHandler(int);
    static SocketEngine* activeInstance;
};

#endif // ENGINE_H