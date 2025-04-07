# Changes resulting from the integration process

## Overview
This document highlights the main changes made during the integration process of the Communicator, Protocol, NIC and SocketEngine classes.

## General Changes

- All attributes relating to size, whether message, buffer or list, are now set to `unsigned int`. This should be the default from now on.
- Threads must be POSIX. In this way, any previous thread implementation has been modified to the new standard.

## Key Changes

### Message
- Template definition now takes an unsigned int to define the `Message::MAX_SIZE`, limiting message length. 
``` cpp
    template <unsigned int MaxSize>
    class Message;
```
- `Message::_data` attribute becomes a void*, allowing any type of data to be passed into a message.

### Ethernet
- Class `Ethernet::Address` has become a `struct Ethernet::Address`, which has only the bytes of the MAC address. This is because `Ethernet::Frame` makes use of `Ethernet::Address`, and, because it is defined as `__attribute__((packed))`, using the physical address as a class was resulting in compilation errors. With this new implementation, `Ethernet::Address` becomes a POD (Plain Old Data), which can be used inside the frame.
- In order to improve the display of the physical address, a `static std::string mac_to_string(Address addr)` method has been defined to make it easier to convert an `Ethernet::Address` to a string and print it out.
```cpp
    static std::string mac_to_string(Address addr) {
        std::ostringstream oss;
        for (unsigned int i = 0; i < MAC_SIZE; ++i) {
            if (i != 0) oss << ":";
            oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(addr.bytes[i]);
        }
        return oss.str();
    }
```

### Debug and Traits
- The purpose of the Debug class is to allow the system to be debugged at runtime. The class is basically a text printing wrapper, which can even be configured to print to a specific file, or some other channel. In this way, it is possible to set up separate log files for each vehicle, which makes it easier to understand the execution of the code and the possible problems encountered.
- If you want to debug the Communicator class, for example, you first need to define the Traits for that class.
```cpp
    template<typename Channel>
    struct Traits<Communicator<Channel>> : public Traits<void>
    {
        static const bool debugged = true;
    };
```
- You also need to define which levels will be debugged by the Debug class. There are 4 possible levels: error, warning, info and trace. In the example below, only the info level will be debugged for each activated class.
```cpp
    template<>
    struct Traits<Debug> : public Traits<void>
    {
        static const bool error = false;
        static const bool warning = false;
        static const bool info = true;
        static const bool trace = false;
    };
```
- Once this is done, inside the class, import the debug.h file, and use debugging as shown below: 
```cpp
    db<Protocol>(INF) << "This is some information about the class.";
```
- This implementation was provided in 2022 by professors Márcio Castro and Giovani Gracioli, in the subject Operating Systems I.

### Initializer and Vehicle
- Aiming for a more generic implementation, so as to allow a variety of tests of the API, new versions of the Initializer class and the Vehicle class are being proposed, decoupling the POSIX process from the internal workings of the vehicle.
- Think of the Vehicle class as just the communication interface between the API and a system that makes use of it. Process management is the responsibility of the system that will use this interface, including the control of threads that send and receive messages.
- These classes should not affect the functioning of the API, since the communication endpoint between systems and components will be the Communicator class.

#### Demo.cpp
- In order to exemplify how these classes work, and at the same time demonstrate how the API works, an example has been defined in the tests/demo.cpp file. The idea is that the vehicle with id 1 is responsible for sending messages during a random execution time, while the other vehicles only receive the messages.
- To run it, run the command:
``` bash
    make tests/demo.cpp
    sudo ./bin/demo -v <number_of_vehicles>
```

### Observer x Observed classes
- The Concurrent_Observer and Concurrent_Observed classes now inherit from Conditionally_Data_Observer and Conditionally_Data_Observed, respectively. This was necessary due to a compilation error between the Communicator class and the Protocol class, because the Communicator was trying to connect to the Protocol, but Protocol::_observer was a Conditionally_Data_Observed and would not accept being observed by a Concurrent_Observer.
- This change, as well as solving the compilation problem, reduces the risk of conflicts in future modifications. The main difference is that the methods are implemented in the basses and overridden, as necessary, in the derived classes.

### SocketEngine
- Use of epoll, with a thread to check the signals received by the Kernel.
- Addition of a method to define the callback function, called after receiving data on the socket.
```cpp
    using CallbackMethod = std::function<void(Ethernet::Frame&, unsigned int)>;
```
- NIC MAC address defined on `SocketEngine::setUpSocket()`

### NIC
- New constant N_BUFFERS, which defines the number of buffers in the pool. BUFFER_SIZE is multiplying the number of send and receive buffers by the size of the Buffers, which resulted in a pool with extremely large and unnecessary values, which increased the application's memory consumption. An investigation is needed to understand the motivation behind this line of code, defined by the teacher.
```cpp
    static const unsigned int N_BUFFERS = Traits<NIC<Engine>>::SEND_BUFFERS + Traits<NIC<Engine>>::RECEIVE_BUFFERS;
```
- In the constructor now, in addition to creating the buffers and initializing the semaphore, we now have the definition of the callback method,  plus, of course, the definitions of the debug messages.
```cpp
    template <typename Engine>
    NIC<Engine>::NIC() {
        db<NIC>(TRC) << "NIC<Engine>::NIC() called!\n";

        (...)

        this->setCallback(std::bind(&NIC::receiveData, this, std::placeholders::_1, std::placeholders::_2));
    }
```
- `NIC::receive()` now extracts the `Ethernet::Frame` from the buffer, and places the payload (a `Protocol::Packet`) in the variable `void* data`, passed as a parameter. Returns the size of the payload. 

### Protocol
- Defined getters and setters for `Address::_paddr` and `Address::_port`. Previously, access was done with public attributes, which violated object-oriented principles.
- The send and receive methods were static. Now they are not anymore. It is assumed that, since Protocol is a singleton, only one instance will exist per vehicle. Therefore, send and receive do not need to be class methods, and can be accessed by `Communicator::_channel`
- `Protocol::receive()` now extracts the `Protocol::Packet` from the frame extracted by NIC, and copies the packet data to the `void* data`, passed as a parameter. Returns the size of the payload (in this case, size of the data contained by the packet)

### Communicator
- Removed the mutex previously used in the class. At the moment, there is still uncertainty as to whether only one Communicator will be used per vehicle or whether there will be a Communicator for each component, in addition to the vehicle's one. For the current delivery (P1), concurrency control is not necessary, therefore, the decision to use the mutex is on standby.
- `Communicator::receive()` now creates a message at the pointer passed as a parameter by the user, with the data, setted by the Protocol, and size, returned by `Protocol::receive()`. Returns the message size.
