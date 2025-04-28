
# Description of the SocketEngine Class (Updated)

## Overview

This document describes the implementation of SocketEngine, the class responsible for managinf a raw socket handling the SIGIO signal and fowarding received messages or broadcasting them through the ethernet. The class stores a callback method to refer to when a message arrives.

## ActiveInstance

Active instance is a private static attribute that points to the one active instance of SocketEngine in the process and enables the signal handler method to call `void SocketEngine::asyncReceive`

## SIGIO

SIGIO is used in SocketEngine in order to enable the asynchronous behavior when receiving a message. In order to achieve this a there are a few requirements:
- The socket is set in non-blocking mode
- The proccess is set as owner of the socket
- The O_ASYNC flag is added to the socket descriptor file
- A method is asigned as signalhandler (`void SocketEngine::SignalHandler(int)`)

If all is correctly setup whenerever a message arrives the kernel sends a signal that interrupts the process execution calling the signal handler method on on of the threads in the process. The SocketEngine instance is then responsable for receiving the packet and forwarding it to th e NIC.
The signal handler is a static method member of the SocketEngine class and while the program is executing in its context it should only make signal safe function calls and operations. That is, complex processing that involves memory allocation, aquiring a mutex, buffered I/O, shell based calls and other should not be done inside the signal handler's contex. While inside the signal handler's contex the message should only be forwarded until it reaches the communicator class that makes it available to to the vehicles' main thread.
```
Kernel
    | 
    |--> sends signal interruption
            |
            |--> signal handler calls the async receive method
                    |
                    |--> async receive recover the message
                    |
                    |--> async receive calls the set callback function set by NIC
                            |
                            |--> NIC notifies the Protocol
                                    |
                                    |--> Protocol updates
                                    |
                                    |--> Protocol notifies the Communicator
                                        |
                                        |--> Communicator updates making the message available to the vehicle

```

## send

Currently the send method takes a `const void* data` data parameter, builds an ethernet header and broadcasts it. The header is 14 bytes long. After validation this method should just store the header data into the Ethernet::Frame struct that will be fed by the NIC as the data parameter.