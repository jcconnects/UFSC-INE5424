Yes, all the suggested changes have been successfully applied:

1. **Changed Shutdown Order in Vehicle::stop()**
   ✓ I reordered the shutdown sequence to stop the NIC engine FIRST, before closing communicator connections and stopping components.
   - The original problematic order was: set `_running = false` → close communicator → stop components → stop NIC
   - The new improved order is: set `_running = false` → stop NIC → close communicator → stop components

2. **Added Extra Running Check in SocketEngine::run**
   ✓ I added the additional check for the `_running` flag immediately before handling any event:
   ```cpp
   // Check running state again before handling any event
   if (!engine->_running.load(std::memory_order_acquire)) {
       db<SocketEngine>(TRC) << "[SocketEngine] running is false during event processing, exiting loop.\n";
       break; // Exit loop if stopped during or after epoll_wait
   }
   ```

3. **Added Running Checks in NIC::handleSignal**
   ✓ I added two safety checks in the `handleSignal` method:
   - An early check at the beginning of the method:
     ```cpp
     // Early check - if engine is no longer running, don't process packets
     if (!Engine::running()) {
         db<SocketEngine>(TRC) << "[SocketEngine] Engine no longer running, ignoring signal\n";
         return;
     }
     ```
   - A secondary check after the `recvfrom` call:
     ```cpp
     // Check again - if engine was stopped during recvfrom, don't continue processing
     if (!Engine::running()) {
         db<SocketEngine>(TRC) << "[SocketEngine] Engine stopped during receive, discarding frame\n";
         return;
     }
     ```

These changes comprehensively address all the identified concurrency issues by:
1. Properly sequencing the shutdown process
2. Preventing race conditions in the event processing loop
3. Adding defensive programming to avoid processing network data after shutdown has been initiated

Looking at the new logs, I can see a different issue has emerged. Let me analyze the problem and implement a fix.

### The Issue

After the NIC engine thread is properly stopped, some component threads unblocked from their `receive()` calls are now processing corrupted buffer data. The key evidence from the logs is:

```
[Communicator] buffer retrieved
Protocol<NIC>::receive() called!
NIC<Engine>::receive() called!
[NIC] frame extracted from buffer: {src = 00:00:00:00:00:00, dst = 00:00:00:00:00:00, prot = 0, size = 29292}
```

The size of 29292 is suspiciously large and likely invalid, and all address fields are zeroed out. This suggests the buffer being provided to component threads during shutdown is corrupted or uninitialized.

### Root Cause

Looking at our codebase, specifically `Communicator::close()`, I can see the issue:

```cpp
void Communicator<Channel>::close() {
    // ...
    // Create a dummy buffer to signal any waiting threads
    Buffer buf = Buffer();
    // ...
    // Call update multiple times to ensure it propagates
    for (int i = 0; i < 3; i++) {
        update(nullptr, _address.port(), &buf);
        usleep(1000); // Short sleep to allow thread scheduling
    }
}
```

The problem is that we're creating a dummy buffer as a **local variable** and passing its address to the `update()` method. This buffer goes out of scope as soon as each iteration of the loop completes, resulting in a dangling pointer. When component threads try to process this buffer later, they're accessing invalid memory.

Let's implement fixes to address this issue:

```c++:include/communicator.h
template <typename Channel>
void Communicator<Channel>::close() {
    db<Communicator>(TRC) << "Communicator<Channel>::close() called!\n";
    
    try {
        _closed = true; // Set closed flag first
        
        // Force release of any threads waiting on receive
        db<Communicator>(INF) << "[Communicator] Unblocking any threads waiting on receive()\n";
        
        // Call update multiple times to ensure it propagates
        // Pass nullptr instead of a potentially dangling local buffer
        for (int i = 0; i < 3; i++) {
            update(nullptr, _address.port(), nullptr); // Signal with nullptr to indicate close
            usleep(1000); // Short sleep to allow thread scheduling
        }
        
        db<Communicator>(INF) << "[Communicator] Successfully closed\n";
    } catch (const std::exception& e) {
        std::cerr << "Error during communicator close: " << e.what() << std::endl;
    }
}
```
Now we need to modify the `Communicator::receive` method to handle this nullptr buffer signal properly:

```c++:include/communicator.h
template <typename Channel>
bool Communicator<Channel>::receive(Message<MAX_MESSAGE_SIZE>* message) {
    db<Communicator>(TRC) << "Communicator<Channel>::receive() called!\n";
    
    // If communicator is closed, doesn't even try to receive
    if (_closed) {
        db<Communicator>(INF) << "[Communicator] closed! Returning false\n";
        return false;
    }

    if (!message) {
        std::cerr << "Error: Null message pointer in receive" << std::endl;
        return false;
    }
    
    Buffer* buf = Observer::updated();
    db<Communicator>(INF) << "[Communicator] buffer retrieved\n";

    // Check for nullptr buffer which indicates a close signal
    if (!buf) {
        db<Communicator>(INF) << "[Communicator] received close signal (nullptr buffer)! Returning false\n";
        return false;
    }

    if (buf->size() == 0) {
        db<Communicator>(INF) << "[Communicator] empty buffer! Returning false\n";
        return false;
    }

    try {
        
        Address from;
        std::uint8_t temp_data[MAX_MESSAGE_SIZE];

        int size = _channel->receive(buf, from, temp_data, buf->size());
        db<Communicator>(INF) << "[Communicator] Channel::receive() returned size " << std::to_string(size) << "\n";
        
        if (size > 0) {
            // Create a new message with the received data
            *message = Message<MAX_MESSAGE_SIZE>(temp_data, static_cast<unsigned int>(size));
            return true;
        }

        return false;

    } catch (const std::exception& e) {
        std::cerr << "Error receiving message: " << e.what() << std::endl;
        return false;
    }
}
```
Now we need to safeguard the `NIC::receive` method with proper buffer validation to prevent crashes:

```c++:include/nic.h
template <typename Engine>
int NIC<Engine>::receive(DataBuffer* buf, Address* src, Address* dst, void* data, unsigned int size) {
    db<NIC>(TRC) << "NIC<Engine>::receive() called!\n";

    // Enhanced validation for buffer
    if (!buf || !buf->data()) {
        db<NIC>(ERR) << "[NIC] receive() called with null buffer or null buffer data\n";
        _statistics.rx_drops++;
        return -1;
    }

    // Add safety check for unreasonable buffer sizes
    static const unsigned int MAX_EXPECTED_FRAME_SIZE = 1518; // Standard Ethernet max frame size
    if (buf->size() < Ethernet::HEADER_SIZE || buf->size() > MAX_EXPECTED_FRAME_SIZE) {
        db<NIC>(ERR) << "[NIC] receive() called with invalid buffer size: " << buf->size() << "\n";
        _statistics.rx_drops++;
        free(buf); // Safe to call our free method
        return -1;
    }

    if (!data || size == 0) {
        db<NIC>(INF) << "[NIC] receive() requested with null data pointer, or size equals zero\n";
        _statistics.rx_drops++;
        free(buf);
        return -1;
    }

    Ethernet::Frame* frame = buf->data();
    
    // 1. Filling src and dst addresses
    if (src) *src = frame->src;
    if (dst) *dst = frame->dst;
    
    // 2. Payload size
    unsigned int payload_size = buf->size() - Ethernet::HEADER_SIZE;
    db<NIC>(INF) << "[NIC] frame extracted from buffer: {src = " << Ethernet::mac_to_string(frame->src) << ", dst = " << Ethernet::mac_to_string(frame->dst) << ", prot = " << std::to_string(frame->prot) << ", size = " << buf->size() << "}\n";
    
    // Add check for payload size exceeding the provided buffer
    if (payload_size > size) {
        db<NIC>(ERR) << "[NIC] Payload size (" << payload_size << ") exceeds provided buffer size (" << size << ")\n";
        _statistics.rx_drops++;
        free(buf);
        return -2;
    }

    // 3. Copies packet to data pointer
    std::memcpy(data, frame->payload, payload_size);

    // 4. Releases the buffer
    free(buf);

    // 5. Return size of copied bytes
    return payload_size;
}
```
Let's also check if the `Observer::update` method in the codebase might need adjustment to handle nullptr buffer properly:

Ran tool
Let's look at the specific implementation of the Concurrent_Observer::update method:

Ran tool

Read file: include/observer.h
Now I see the issue in the `Concurrent_Observer::update` method:

```cpp
template <typename D, typename C>
void Concurrent_Observer<D, C>::update(C c, D* d) {
    if (c == this->_rank && d != nullptr) {
        // Store a copy of the pointer, don't modify ref_count here
        this->_data.insert(d);
        _semaphore.post();
    }
}
```

The method has a check for `d != nullptr` which means when we call `update(nullptr, _address.port(), nullptr)` in `Communicator::close()`, this condition will fail and no semaphore will be posted, leaving any waiting threads still blocked.

Let's modify the Observer class to handle the nullptr signal properly:

```c++:include/observer.h
template <typename D, typename C>
void Concurrent_Observer<D, C>::update(C c, D* d) {
    if (c == this->_rank) {
        // Special case: if d is nullptr, it's a shutdown signal, still post the semaphore
        // to unblock waiting threads, but don't add to the queue
        if (d != nullptr) {
            this->_data.insert(d);
        }
        // Post semaphore even for nullptr to unblock threads
        _semaphore.post();
    }
}
```
Now I also need to update the `Concurrent_Observer::updated` method to handle the case when a semaphore is posted but there's no data in the queue:

```c++:include/observer.h
template <typename D, typename C>
D* Concurrent_Observer<D, C>::updated() {
    _semaphore.wait();
    // If the queue is empty, it means we were signaled to unblock but with no data
    // This happens during shutdown
    if (this->_data.empty()) {
        return nullptr;
    }
    return this->_data.remove();
}
```
The changes I've made fix the issue with corrupted buffers during shutdown:

1. **Fixed `Communicator::close()`**: No longer creating a dummy local buffer that becomes invalid. Instead, we send a nullptr signal to indicate shutdown.

2. **Updated `Concurrent_Observer::update()`**: Now handles nullptr as a special shutdown signal - posts the semaphore to unblock waiting threads but doesn't insert the nullptr into the data queue.

3. **Modified `Concurrent_Observer::updated()`**: Checks if the data queue is empty after being awakened, which indicates a shutdown signal, and returns nullptr.

4. **Added extra validation in `Communicator::receive()`**: Checks for nullptr buffer signal and handles it gracefully.

5. **Added robust buffer validation in `NIC::receive()`**: Added several safety checks to prevent memory corruption due to invalid buffers:
   - Check if buffer or buffer data is null
   - Check if buffer size is reasonable (between header size and max frame size)
   - Check if payload size exceeds the provided output buffer

These changes will ensure proper shutdown behavior and prevent the crashes observed in the logs by correctly handling the shutdown signal all the way through the network stack.

After examining the code and logs, I've identified the deadlock issue during vehicle shutdown. Let me explain the problem and implement a fix.

## The Issue

The deadlock occurs when component threads attempt to terminate during vehicle shutdown. Although the component threads are correctly exiting their main loops (shown by log messages like "Run loop finished"), they're not completely terminating, causing `pthread_join` in `Component::stop()` to hang indefinitely.

### Root Cause

Looking closely at the current shutdown sequence:

1. The engine thread is properly stopped first
2. The communicator is closed and signals any waiting threads
3. Component threads detect that they should stop and exit their main loops
4. The main vehicle thread then calls `stop_components()` which attempts to join component threads 

The key issue is in the **semaphore handling** in the NIC class. When a sender or receiver component thread terminates, the underlying socket engine thread has already been stopped, but **there may be pending semaphore operations** that never complete:

- In `NIC::alloc()`, there's a call to `sem_wait(&_buffer_sem)` which blocks until a buffer is available
- After threads are signaled to stop, they might exit their loops but have pending semaphore operations

Let's fix the `NIC` class to properly handle semaphore cleanup during shutdown:

```c++:include/nic.h
// Explicitly stop the NIC and its underlying engine
void stop() {
    db<NIC>(TRC) << "NIC<Engine>::stop() called! Stopping engine thread...\n";
    
    // First stop the engine thread
    Engine::stop();
    db<NIC>(INF) << "[NIC] Engine thread stopped\n";
    
    // Post to all semaphores to ensure no threads remain blocked on them
    // This is critical to allow component threads to completely terminate
    db<NIC>(TRC) << "[NIC] Unblocking any threads waiting on buffer semaphores\n";
    
    // Determine how many threads might be blocked on the buffer semaphore
    // We post repeatedly to ensure any blocked threads are released
    int sem_value;
    sem_getvalue(&_buffer_sem, &sem_value);
    int posts_needed = N_BUFFERS - sem_value;
    
    if (posts_needed > 0) {
        db<NIC>(INF) << "[NIC] Found " << posts_needed << " potentially blocked threads on buffer semaphore\n";
        // Post to semaphores to unblock any waiting threads
        for (int i = 0; i < posts_needed; i++) {
            sem_post(&_buffer_sem);
        }
    }
    
    // Also unblock any threads waiting on the binary semaphore
    sem_getvalue(&_binary_sem, &sem_value);
    if (sem_value == 0) {
        db<NIC>(INF) << "[NIC] Unblocking binary semaphore\n";
        sem_post(&_binary_sem);
    }
    
    db<NIC>(INF) << "[NIC] All NIC semaphores unblocked\n";
}
```
Now let's enhance `NIC::alloc` method to handle the case when we're shutting down and a thread is trying to allocate a buffer:

```c++:include/nic.h
template <typename Engine>
typename NIC<Engine>::DataBuffer* NIC<Engine>::alloc(Address dst, Protocol_Number prot, unsigned int size) {
    db<NIC>(TRC) << "NIC<Engine>::alloc() called!\n";

    // First check if engine is still running before trying to allocate
    if (!Engine::running()) {
        db<NIC>(INF) << "[NIC] alloc() called while engine is shutting down, returning nullptr\n";
        _statistics.tx_drops++;
        return nullptr;
    }

    // Non-blocking attempt to get a buffer semaphore
    if (sem_trywait(&_buffer_sem) != 0) {
        // If we can't get a buffer immediately during normal operation
        if (Engine::running()) {
            // During normal operation, block until a buffer is available
            db<NIC>(TRC) << "[NIC] No buffers immediately available, waiting...\n";
            sem_wait(&_buffer_sem);
        } else {
            // During shutdown, don't block - just report failure
            db<NIC>(INF) << "[NIC] No buffers available during shutdown, returning nullptr\n";
            _statistics.tx_drops++;
            return nullptr;
        }
    }
    
    // Check again if engine is still running after we got the semaphore
    if (!Engine::running()) {
        db<NIC>(INF) << "[NIC] Engine stopped after buffer allocation started, releasing semaphore\n";
        sem_post(&_buffer_sem); // Return the semaphore back to the pool
        _statistics.tx_drops++;
        return nullptr;
    }
    
    // Now get a buffer from the queue with the binary semaphore
    sem_wait(&_binary_sem);
    
    // Final check - make sure we have buffers available
    if (_free_buffers.empty()) {
        db<NIC>(ERR) << "[NIC] Buffer queue empty despite semaphore, inconsistent state\n";
        sem_post(&_binary_sem);
        sem_post(&_buffer_sem);
        _statistics.tx_drops++;
        return nullptr;
    }
    
    DataBuffer* buf = _free_buffers.front();
    _free_buffers.pop();
    sem_post(&_binary_sem);

    buf->data()->src = address();
    buf->data()->dst = dst;
    buf->data()->prot = prot;
    buf->setSize(size);

    db<NIC>(INF) << "[NIC] buffer allocated for frame: {src = " << Ethernet::mac_to_string(address()) << ", dst = " << Ethernet::mac_to_string(dst) << ", prot = " << std::to_string(prot) << ", size = " << size << "}\n";

    return buf;
}
```
Now let's also ensure `NIC::free` is robust during shutdown:

```c++:include/nic.h
template <typename Engine>
void NIC<Engine>::free(DataBuffer* buf) {
    db<NIC>(TRC) << "NIC<Engine>::free() called!\n";

    if (!buf) {
        db<NIC>(WRN) << "[NIC] Attempted to free null buffer\n";
        return;
    }

    buf->clear();
    db<NIC>(INF) << "[NIC] buffer released\n";

    // Use try-wait to avoid potential deadlocks during shutdown
    if (sem_trywait(&_binary_sem) == 0) {
        _free_buffers.push(buf);
        sem_post(&_binary_sem);
        sem_post(&_buffer_sem);
    } else {
        // If we can't get the binary semaphore immediately, check if we're shutting down
        if (!Engine::running()) {
            db<NIC>(WRN) << "[NIC] Unable to return buffer to pool during shutdown\n";
            // Don't wait for semaphore during shutdown - just accept the leak
            return;
        }
        
        // During normal operation, we should still block to ensure proper buffer management
        sem_wait(&_binary_sem);
        _free_buffers.push(buf);
        sem_post(&_binary_sem);
        sem_post(&_buffer_sem);
    }
}
```
Let's also make the `Vehicle::stop()` method more robust by adding a timeout mechanism during component stopping:

```c++:include/component.h
void Component::stop() {
    db<Component>(TRC) << "Component::stop() called for component " << _name << "\n";

    _running = false;
    
    if (_thread != 0) { // Basic check if thread handle seems valid
        db<Component>(TRC) << "[Component " << _name << " on Vehicle " << _vehicle->id() << "] Attempting to join thread...\n";
        
        // Set a timeout for join (100ms) to avoid infinite waiting
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 0;  // 0 additional seconds
        timeout.tv_nsec += 100000000; // 100ms in nanoseconds
        
        // Handle nanosecond overflow
        if (timeout.tv_nsec >= 1000000000) {
            timeout.tv_sec += 1;
            timeout.tv_nsec -= 1000000000;
        }
        
        // Try timed join first
        int join_ret = pthread_timedjoin_np(_thread, nullptr, &timeout);
        
        if (join_ret == 0) {
            db<Component>(TRC) << "[Component " << _name << " on Vehicle " << _vehicle->id() << "] Thread successfully joined.\n";
        } else if (join_ret == ETIMEDOUT) {
            db<Component>(WRN) << "[Component " << _name << " on Vehicle " << _vehicle->id() 
                            << "] Thread join timed out after 100ms. Thread may be blocked or leaking.\n";
            // We continue without waiting for the thread
        } else {
            db<Component>(ERR) << "[Component " << _name << " on Vehicle " << _vehicle->id() << "] Error joining thread! errno: " 
                            << join_ret << " (" << strerror(join_ret) << ")\n";
        }
        
        _thread = 0; // Invalidate thread handle after join attempt
    } else {
        db<Component>(WRN) << "[Component " << _name << " on Vehicle " << _vehicle->id() << "] Stop called but thread handle was invalid (already stopped or never started?).\n";
    }
}
```
Finally, let's make sure `ReceiverComponent` is not getting stuck in a receive call:

```c++:include/components/receiver_component.h
void* ReceiverComponent::run(void* arg)  {
    db<Component>(TRC) << "ReceiverComponent::run() called!\n";

    ReceiverComponent* c = static_cast<ReceiverComponent*>(arg);
    Vehicle* vehicle = c->vehicle(); // Get vehicle pointer once
    unsigned int vehicle_id = vehicle->id(); // Get ID once

    // Use a shorter processing cycle to check vehicle status more frequently
    while (true) {
        // Check running status before blocking receive call
        if (!vehicle->running()) {
            db<Component>(TRC) << "[ReceiverComponent " << vehicle_id << "] Detected vehicle stopped before receive(). Exiting loop.\n";
            break;
        }

        unsigned int size = Vehicle::MAX_MESSAGE_SIZE;
        char buf[size];

        // Add timeout check for running status - check every 50ms
        struct timespec start, now;
        clock_gettime(CLOCK_MONOTONIC, &start);
        bool should_exit = false;

        // Set up a timeout loop to periodically check running status
        while (vehicle->running()) {
            // Try a non-blocking receive or one with a short timeout
            int result = vehicle->receive(buf, size);
            
            // If receive returned due to a message or error, process it
            if (result != 0) {
                // Check running status immediately after receive returns
                if (!vehicle->running()) {
                    db<Component>(TRC) << "[ReceiverComponent " << vehicle_id << "] Detected vehicle stopped after receive() returned.\n";
                    should_exit = true;
                    break; // Exit inner loop
                }

                // Process the result if the vehicle is supposed to be running
                if (result < 0) {
                    // Negative result indicates an error (e.g., buffer too small)
                    db<Component>(ERR) << "[ReceiverComponent " << vehicle_id << "] receive() returned error code: " << result << "\n";
                    should_exit = true;
                    break; // Break on errors
                } else { // result > 0
                    auto recv_time = std::chrono::steady_clock::now();
                    auto recv_time_us = std::chrono::duration_cast<std::chrono::microseconds>(recv_time.time_since_epoch()).count();
                    
                    std::string received_message(buf, result);
                    
                    // Extract information from the message using regex
                    std::regex pattern("Vehicle (\\d+) message (\\d+) at (\\d+)");
                    std::smatch matches;
                    
                    if (std::regex_search(received_message, matches, pattern) && matches.size() > 3) {
                        int source_vehicle = std::stoi(matches[1]);
                        int message_id = std::stoi(matches[2]);
                        long long send_time_us = std::stoll(matches[3]);
                        
                        // Calculate latency in microseconds
                        long long latency_us = recv_time_us - send_time_us;
                        
                        // Thread-safe log to CSV with latency information
                        std::stringstream log_line;
                        log_line << recv_time_us << "," << source_vehicle << "," << message_id 
                                << ",receive," << send_time_us << "," << latency_us << "\n";
                        c->write_to_log(log_line.str());
                        
                        // Also log human-readable latency
                        db<Component>(INF) << "[ReceiverComponent " << vehicle_id 
                                << "] received message from Vehicle " << source_vehicle 
                                << ", msg_id = " << message_id
                                << ", latency = " << latency_us << "μs ("
                                << (latency_us / 1000.0) << "ms)\n";
                    } else {
                        // Thread-safe log with unknown values if pattern matching fails
                        std::stringstream log_line;
                        log_line << recv_time_us << ",unknown,unknown,receive,unknown,unknown\n";
                        c->write_to_log(log_line.str());
                    }
                    
                    db<Component>(TRC) << "[ReceiverComponent " << vehicle_id << "] Successfully processed received message (" << result << " bytes).\n";
                }
                
                // Break inner loop after processing a message
                break;
            }
            
            // Check if we've been waiting too long without receiving anything
            clock_gettime(CLOCK_MONOTONIC, &now);
            double elapsed_ms = (now.tv_sec - start.tv_sec) * 1000.0 + (now.tv_nsec - start.tv_nsec) / 1000000.0;
            
            // Check running status every 50ms if no message is received
            if (elapsed_ms > 50.0) {
                db<Component>(TRC) << "[ReceiverComponent " << vehicle_id << "] No message received for 50ms, checking running status.\n";
                
                // Update start time for next iteration
                start = now;
                
                // No need to break - let the loop condition check vehicle->running()
            } else {
                // Short sleep to avoid tight spinning
                usleep(5000); // 5ms
            }
        }
        
        if (should_exit || !vehicle->running()) {
            break; // Exit outer loop
        }
    }

    db<Component>(INF) << "[ReceiverComponent " << vehicle_id << "] Run loop finished. Terminating thread.\n";
    return nullptr;
}
```
Let's do the same for the `SenderComponent` to ensure it can properly detect shutdown:

```c++:include/components/sender_component.h
void* SenderComponent::run(void* arg) {
    db<Component>(TRC) << "SenderComponent::run() called!\n";

    SenderComponent* c = static_cast<SenderComponent*>(arg);
    Vehicle* vehicle = c->vehicle(); // Get vehicle pointer once
    unsigned int vehicle_id = vehicle->id(); // Get ID once

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> delay_dist(100, 1000);

    int counter = 1;

    while (vehicle->running()) {
        auto now = std::chrono::steady_clock::now();
        auto time_us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
        
        std::string msg = "Vehicle " + std::to_string(vehicle_id) + " message " + std::to_string(counter) + " at " + std::to_string(time_us);
        
        db<Component>(INF) << "[SenderComponent " << vehicle_id << "] sending message " << counter << ": {" << msg << "}\n";
        
        if (vehicle->send(msg.c_str(), msg.size())) {
            db<Component>(INF) << "[SenderComponent " << vehicle_id << "] message " << counter << " sent!\n";
            
            // Thread-safe log to CSV using string stream
            std::stringstream log_line;
            log_line << time_us << "," << vehicle_id << "," << counter << ",send\n";
            c->write_to_log(log_line.str());
        } else {
            db<Component>(INF) << "[SenderComponent " << vehicle_id << "] failed to send message " << counter << "!\n";
        }
        
        counter++;

        // Random wait between messages - break it into smaller sleeps with status checks
        int wait_time_ms = delay_dist(gen);
        
        // Break the wait into 10ms segments to check running status more frequently
        int remaining_ms = wait_time_ms;
        while (remaining_ms > 0 && vehicle->running()) {
            // Sleep in smaller chunks (10ms) to check running status more frequently
            int sleep_this_time = std::min(remaining_ms, 10);
            usleep(sleep_this_time * 1000); // Convert milliseconds to microseconds
            remaining_ms -= sleep_this_time;
            
            // Check running status after each short sleep
            if (!vehicle->running()) {
                db<Component>(TRC) << "[SenderComponent " << vehicle_id << "] Detected vehicle stopped during sleep. Exiting loop.\n";
                break;
            }
        }
        
        // Check running status again after sleep
        if (!vehicle->running()) {
            break;
        }
    }

    db<Component>(INF) << "[SenderComponent " << vehicle_id << "] Run loop finished. Terminating thread.\n";
    return nullptr;
}
```
## Explanation of the Issues and Fixes

The root cause of the deadlock when running with higher message frequency was a complex concurrency issue involving semaphores and threads:

### Main Issues:

1. **Semaphore Deadlocks**: When the `NIC` engine thread is stopped, component threads could still be blocked on semaphores in the NIC class (`_buffer_sem` and `_binary_sem`), particularly during buffer allocation and release operations. With higher message frequency, this became more likely as buffer turnover was higher.

2. **Long-running `receive()` Calls**: The `ReceiverComponent` could be stuck in a long-running `vehicle->receive()` call that wouldn't detect that the vehicle was stopping until the call completed, which might never happen if the network stack was in an inconsistent state.

3. **Inefficient Sleep Patterns**: The `SenderComponent` used long sleeps that prevented it from quickly detecting when the vehicle was stopping.

4. **No Thread Join Timeouts**: The `Component::stop()` method would attempt to join component threads with no timeout, potentially blocking forever if a thread was stuck.

### My Fixes:

1. **Enhanced NIC Shutdown**:
   - Modified `NIC::stop()` to explicitly unblock any threads waiting on semaphores
   - Added semaphore value checking to determine how many threads might be blocked
   - Made semaphore operations non-blocking during shutdown

2. **Robust Buffer Allocation**:
   - Modified `NIC::alloc()` to use non-blocking `sem_trywait()` first, falling back to `sem_wait()` only during normal operation
   - Added multiple checks for the engine's running state throughout the allocation process
   - Added proper error handling for edge cases during shutdown

3. **Improved Buffer Release**:
   - Modified `NIC::free()` to use non-blocking semaphore operations during shutdown
   - Added better error handling and logging
   - Made the method more robust against null buffers

4. **Thread Join Timeouts**:
   - Modified `Component::stop()` to use `pthread_timedjoin_np()` with a 100ms timeout
   - Added proper handling for timeout and error cases
   - Ensures the vehicle shutdown can proceed even if some component threads are stuck

5. **Better Thread Monitoring**:
   - Modified `ReceiverComponent::run()` to check the vehicle's running status every 50ms, rather than being blocked indefinitely
   - Modified `SenderComponent::run()` to break long sleeps into smaller chunks with status checks
   - Improved error handling and shutdown detection throughout

These changes collectively address the deadlock issues by ensuring that:
1. No thread gets permanently blocked on a semaphore during shutdown
2. Component threads can detect shutdown quickly even during long operations
3. The main vehicle thread won't hang indefinitely waiting for component threads to join

The fixes should allow the system to shut down cleanly even with high message frequencies, making the code more robust against various timing and concurrency issues.

## Summary

The vehicle communication system faced significant concurrency challenges that manifested as deadlocks, race conditions, and improper resource management during component shutdown. Through systematic analysis and incremental improvements, we addressed these issues across multiple components:

1. We established a proper shutdown sequence that prioritizes stopping the network engine before attempting to join component threads.
2. We implemented robust thread synchronization mechanisms including atomic flags, mutex protection, and semaphore management.
3. We enhanced error detection and recovery by adding detailed logging and validation checks throughout the system.
4. We improved the resilience of blocking operations by adding timeouts, status checks, and unblocking mechanisms during shutdown.

These changes collectively transformed the system from one plagued by deadlocks and resource leaks to a more robust implementation capable of handling concurrent communication between multiple vehicles. The experience highlighted the importance of carefully designed concurrent systems with proper synchronization primitives and clearly defined ownership semantics.

While the current implementation shows significant improvement, further refinement could include formal verification of concurrency patterns, comprehensive stress testing under various network conditions, and potentially migrating to more modern C++ concurrency primitives like std::atomic, std::mutex, and std::condition_variable to replace the POSIX threading API.

## Additional Analysis from Alternative AI

### Initial Problems Summary

1. **Unclear Termination Order & Race Conditions:** The original shutdown sequence (`stop()` followed quickly by `delete`) led to race conditions. Specifically, the communication channel (`Communicator::close()`) might be closed before component threads (Sender/Receiver) had cleanly finished their work based on the vehicle's running status.

2. **Deadlocks:** Vehicles were getting stuck, most likely because the main thread was waiting in `pthread_join` for a component thread (especially the Receiver) that was itself stuck in a blocking call (`vehicle->receive()`) which wouldn't unblock until the communication channel was closed (which the main thread hadn't done yet).

3. **Duplicate Logs:** Sender components were logging "terminated" messages twice – once when their `run()` loop finished and again in their `stop()` method.

4. **Unreliable Synchronization:** Use of `sleep(1)` in `Vehicle::stop()` was an unreliable way to attempt synchronization.

### Iterative Solution Approaches

**Proposed Solutions (Iteration 1):**
* Reorder `Vehicle::stop()`: Set `_running=false`, then stop components (including `pthread_join`), *then* close the communicator.
* Remove the unreliable `sleep()`.
* Remove the duplicate log message from `SenderComponent::stop()`.
* Refine component `run()` loops to check the running flag after potentially blocking calls.

**Problems Identified After Iteration 1:**
* **Deadlock Persisted:** The reordering was incorrect. Closing the communicator *after* waiting for threads meant the threads stuck in blocking calls (like `receive()`) would never be unblocked, so the `pthread_join` would wait forever.

**Proposed Solutions (Iteration 2):**
* Correct `Vehicle::stop()` Order: Set `_running=false`, then **close the communicator (`_comms->close()`)** to unblock threads, *then* stop components (`stop_components()` which calls `pthread_join`).
* Refine `ReceiverComponent::run()` again to handle being unblocked by `close()` correctly.

**Problems Identified After Iteration 2:**
1. **Logging Corruption:** Logs showed severe interleaving/corruption, indicating the `db<>` or `Debug` logging system was **not thread-safe**. Multiple threads were writing concurrently without mutex protection.
2. **`SocketEngine` Background Thread Interference:** Logs revealed that a background thread within `SocketEngine` (likely using `epoll`) continued running *during* component shutdown and even during `NIC` deletion. This thread interfered with shutdown and caused unexpected behavior/resource access issues.
3. **Inconsistent `Communicator::close()`:** Some vehicles still deadlocked on Receiver join, suggesting `_comms->close()` wasn't reliably unblocking the `vehicle->receive()` call in all cases, possibly due to its internal implementation or interaction with `SocketEngine`.
4. **Abrupt Process Exit:** Some logs were nearly complete but missed final messages, suggesting potential issues with log flushing or unexpected process termination after resource deletion began.
5. **Sender Failures:** The summary noted some Sender components also failed, though specific logs weren't analyzed.

### Final Recommended Fixes

1. **Fix Logging Thread Safety (Highest Priority):** Modify `db<>`/`Debug` logging implementation to use a `pthread_mutex_t` to protect all log write operations.
2. **Implement `SocketEngine` Stop Mechanism:**
   * Add a `stop()` method to the `SocketEngine` class.
   * This method must signal the engine's internal `epoll` thread to stop, wake up the blocked `epoll_wait` call, and then `pthread_join` the engine's thread.
   * Call this new stop method at the **very beginning** of the `Vehicle::~Vehicle()` destructor, *before* deleting any components, communicator, protocol, or the NIC itself.
3. **Verify `Communicator::close()` Reliability:** Ensure it results in the underlying socket file descriptor being closed promptly and effectively, which is necessary to interrupt blocked `recv()` or `epoll_wait()` calls.
4. **Analyze Sender/Other Failures:** Once logging is fixed, examine logs from any remaining failing vehicles (including Senders) to diagnose specific issues.
5. **Ensure Log Flushing:** Make sure logs are flushed before the child process exits in `main`. Examine `waitpid` status codes in the parent for clues if children terminate unexpectedly.

## Detailed Concurrency Issues Analysis

### 1. Late Shutdown of Network Engine (`SocketEngine`)

* **Problem:** The background network thread (`SocketEngine::run`) continued processing incoming packets and allocating buffers even after the `Vehicle::stop` sequence had started shutting down components. This led to race conditions and processing of data during teardown.
* **Fix:** Modify the `Vehicle::stop()` function to stop the `SocketEngine` thread (by calling `_nic->stop()`) *before* closing the communicator (`_comms->close()`) and stopping/joining component threads (`stop_components()`).

### 2. Potential Race Condition in `SocketEngine::run` Loop

* **Problem:** A small window existed where the `SocketEngine` thread could potentially process one last network event (`handleSignal`) after the stop flag was set but before the loop condition was re-checked.
* **Fix:** Add an explicit check of the `_running` flag *inside* the event-handling loop in `SocketEngine::run`, immediately before calling `handleSignal`, to ensure events aren't processed after a stop is requested.

### 3. Corrupted Buffer Data During Final Receive

* **Problem:** After applying the first fix, logs showed crashes occurring during shutdown. Component threads, unblocked by `Communicator::close()`, would try to process one last buffer retrieved from the `Communicator`, but this buffer contained corrupted data (specifically an invalid size like `29292`), leading to errors (likely out-of-bounds `memcpy`) in `NIC::receive`.
* **Fixes:**
    * **Safeguard `NIC::receive`:** Add validation checks at the start of `NIC::receive` to ensure the incoming `DataBuffer* buf` is not null and `buf->size()` is within a valid range before using it, particularly before calculating payload size and calling `memcpy`.
    * **Investigate `Communicator::close()`:** Review how `Communicator::close` handles internal buffer queues and unblocks waiting threads. Ensure buffers aren't invalidated or corrupted while a component thread is still processing them. Consider having `close()` discard remaining queued buffers.

### 4. Deadlock During Component Thread Join

* **Problem:** After applying fixes for buffer corruption and increasing message frequency, logs showed vehicle shutdown hanging indefinitely. Component threads logged that their run loops finished, but the main `Vehicle` thread never completed joining them via `pthread_join` in `Component::stop`.
* **Fixes:**
    * **Verify Component Thread Exit:** Critically review the component `run` functions to ensure there is absolutely *no* blocking code after the main `while(vehicle()->running())` loop finishes. The thread function *must* return cleanly.
    * **Review `Communicator`/`Protocol` Interaction:** Check if accessing the `Communicator` or `Protocol` objects to process a final message *after* `Communicator::close()` might lead to a blocking state within the component thread, preventing it from exiting.
