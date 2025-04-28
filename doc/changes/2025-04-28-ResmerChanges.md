---
title: "NIC and Engine Changes"
date: 2025-04-28
time: "16:30"
---

# NIC and Engine Changes

## Overview
The changes aim to simplify or improve NIC, SocketEngine and SharedMemoryEngine implementations. The internal communication was simplified and some simple fixes were made.

### 1. NIC setup fix
NIC was not starting the engines before trying to setup epoll, which caused the epoll setup to fail.

### After Changes:
```cpp
// Engines should be started *before* the event loop if they manage their own threads/resources
// Assuming engines have a start() method or are ready after construction
_external_engine.start(); // If needed
_internal_engine.start(); // If needed

setupNicEpoll(); // Sets up _nic_ep_fd and adds engine FDs
```

### 2. NIC handling of internal messages improved
The NIC class relied on epoll observing a timer file descriptor to handle internal messages which was overcomplicated and ineficient. Now it simply calls the internal event handler method after every successfully sent message.

### After Changes
```cpp
// Routing Decision: If destination is self, use internal engine. Otherwise, external.
if (dest_mac == my_mac) {
    db<NIC>(INF) << "[NIC] Routing frame locally via InternalEngine (dst == self)\n";
    // Assumes InternalEngine::send() takes Ethernet::Frame* and size
    result = _internal_engine.send(buf->data(), buf->size());
    if (result > 0) {
        _statistics.packets_sent_internal++;
        _statistics.bytes_sent_internal += result;
        handleInternalEvent(); // Notify internal engine of sent data
    } else {
        _statistics.tx_drops_internal++;
        db<NIC>(WRN) << "[NIC] InternalEngine::send failed (result=" << result << ")\n";
    }
}
```

### 3. Nic Alloc now is non-blocking
Instead of aquiring a semaphore token or waiting for one nic aquires only the binary semaphore and inside its context checks for buffer availability. This implementation enlarges the rece condition scope as it is only possible for one thread to check buffer availability at a time in order to be thread safe.

```cpp
db<NIC>(TRC) << "NIC::alloc() called!\n";

// Check if NIC is running before potentially blocking on semaphore
if (!_running.load(std::memory_order_acquire)) {
    db<NIC>(WRN) << "[NIC] alloc() called when NIC is not running.\n";
    return nullptr;
}

sem_wait(&_binary_sem); // Lock the queue
// Check if there are free buffers available
if (!_free_buffer_count) {
    db<NIC>(WRN) << "[NIC] No free buffers available for allocation.\n";
    sem_post(&_binary_sem); // Release semaphore
    return nullptr; // No buffer available 
}
// TODO - review if this is needed
// Re-check running status after acquiring semaphore, in case stop() was called
if (!_running.load(std::memory_order_acquire)) {
    db<NIC>(WRN) << "[NIC] alloc() acquired semaphore but NIC stopped.\n";
    return nullptr;
}

DataBuffer* buf = _free_buffers.front();
_free_buffers.pop();
_free_buffer_count--; // Decrement available buffer count
sem_post(&_binary_sem); // Unlock the queue
```

### Other Proposed Changes
**No SharedMemoryEngine** NIC can totally handle internal communication without depending on an engine. Even though that seems to imply that the NIC would take over responsabilities from the engine class that is not true at all. Allocating a buffer is enough in case of iternal messaging. So as soon as send is called and the NIC identifies the internal communication it could just immediately call the notifty method.

### Pending Fixes

### Message delivery fix
This is the virtual destination address test:
```cpp
ECU1::send successful (38 bytes to 1).
ECU2::receive received message of size 888 from 00:00:00:00:00:00:0
ECU2::receive successfully processed message (888 bytes copied).
ECU2::receive received message of size 888 from 00:00:00:00:00:00:0
```
The virtual address works well and the message is being delivered to the right component, but the there is something wrong with the integrity of the delivered message. Its source address should not be "00:00:00:00:00:00", its body should not be "0" and its size should not be "888 bytes".

### Termination procedure fix
Currrently neither the demo nor the virtual destination address tests are terminating currectly and the second is falling into a segmentation fault. The changes on the components likely compromised some part of the termination procedure.
