#ifndef SHAREDMEMORYENGINE_H
#define SHAREDMEMORYENGINE_H

#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>    // For mode constants
#include <fcntl.h>       // For O_* constants
#include <semaphore.h>
#include <sys/timerfd.h> // For timerfd
#include <time.h>        // For timespec
#include <atomic>
#include <stdexcept>
#include <string>
#include <iostream> // Include missing iostream

#include "ethernet.h"
#include "traits.h"
#include "debug.h"

// Define constants for Shared Memory and Semaphores
const char* SHM_NAME = "/vehicle_internal_shm";
const char* SEM_MUTEX_NAME = "/vehicle_shm_mutex";
const char* SEM_ITEMS_NAME = "/vehicle_shm_items"; // Counts items in buffer
const char* SEM_SPACE_NAME = "/vehicle_shm_space"; // Counts free slots

// Define structure for data within the shared memory queue
// Needs to hold payload and the original Ethernet protocol number
struct SharedFrameData {
    Ethernet::Protocol protocol; // The L3 protocol number
    unsigned int payload_size;   // Actual size of the payload
    // Use a fixed-size buffer matching the trait MTU
    unsigned char payload[Traits<SharedMemoryEngine>::MTU];
};

// Define the structure for the entire shared memory region
struct SharedRegion {
    // --- Metadata ---
    std::atomic<bool> initialized; // Flag to signal when setup is complete
    std::atomic<uint32_t> ref_count;  // How many processes are attached

    // --- Ring Buffer ---
    static const uint32_t QUEUE_CAPACITY = Traits<SharedMemoryEngine>::BUFFER_SIZE; // Max frames from traits
    uint32_t read_index;    // Index of the next frame to read
    uint32_t write_index;   // Index of the next slot to write to
    SharedFrameData buffer[QUEUE_CAPACITY];

    // --- Padding (Optional) ---
    // Add padding if necessary to avoid issues with adjacent members
    // if semaphores were stored directly here (they are not, using named semaphores).
};

class SharedMemoryEngine {
public:
    SharedMemoryEngine();
    ~SharedMemoryEngine();

    // Prevent copying
    SharedMemoryEngine(const SharedMemoryEngine&) = delete;
    SharedMemoryEngine& operator=(const SharedMemoryEngine&) = delete;

    void start(); // Initialize/Attach to shared resources
    void stop();  // Detach from shared resources, potentially cleanup

    const bool running() const;

    // --- Interface methods required by NIC ---
    // Send data (payload + protocol) into the shared queue
    int send(Ethernet::Frame* frame, unsigned int size);

    // Receive data (payload + protocol) from the shared queue
    // Called by NIC after timerfd notification
    int receiveData(void* payload_buf, unsigned int max_size, Ethernet::Protocol* proto_out);

    // Returns a timerfd for NIC to poll this engine
    int getNotificationFd() const;

    // Internal engine doesn't have a specific MAC, return null/default
    Ethernet::Address getMacAddress() const;
    // ----------------------------------------
    
private:
    void initializeSharedMemory(); // Logic for the first process
    void attachToSharedMemory();   // Logic for subsequent processes
    void cleanupSharedResources(bool is_last_process);
    
private:
    std::atomic<bool> _running;
    int _shm_fd;           // Shared memory file descriptor
    SharedRegion* _shared_region; // Pointer to mapped shared memory
    sem_t* _mutex_sem;     // Named semaphore for mutual exclusion
    sem_t* _items_sem;     // Named semaphore counting items in queue
    sem_t* _space_sem;     // Named semaphore counting free slots in queue
    int _poll_timer_fd;    // Timer FD for NIC polling notification
    bool _is_initializer;  // Was this instance the one that created the SHM?
};

/********** SharedMemoryEngine Implementation **********/

// Constructor: Initialize pointers/FDs to invalid states
SharedMemoryEngine::SharedMemoryEngine()
    : _running(false),
      _shm_fd(-1),
      _shared_region(nullptr),
      _mutex_sem(SEM_FAILED),
      _items_sem(SEM_FAILED),
      _space_sem(SEM_FAILED),
      _poll_timer_fd(-1),
      _is_initializer(false)
{
    db<SharedMemoryEngine>(TRC) << "SharedMemoryEngine::SharedMemoryEngine() called!\n";
}

// Destructor: Ensure resources are released
SharedMemoryEngine::~SharedMemoryEngine() {
    db<SharedMemoryEngine>(TRC) << "SharedMemoryEngine::~SharedMemoryEngine() called!\n";
    stop(); // Ensure cleanup happens
}

// Start: Create/Open Shared Memory and Semaphores, Start Timer
void SharedMemoryEngine::start() {
    db<SharedMemoryEngine>(TRC) << "SharedMemoryEngine::start() called!\n";
    if (running()) {
        db<SharedMemoryEngine>(WRN) << "SharedMemoryEngine already running.\n";
        return;
    }

    bool first_attempt = true;
    while (true) { // Loop to handle potential race condition during creation
        // Try to create exclusively (first process)
        _shm_fd = shm_open(SHM_NAME, O_CREAT | O_EXCL | O_RDWR, 0660);

        if (_shm_fd >= 0) {
            // Success: We are the first process
            _is_initializer = true;
            db<SharedMemoryEngine>(INF) << "[SHM Engine] Creating new shared memory region."
                                         << " (fd=" << _shm_fd << ")";
            try {
                initializeSharedMemory();
                break; // Initialization successful
            } catch (const std::exception& e) {
                db<SharedMemoryEngine>(ERR) << "[SHM Engine] Failed to initialize shared memory: "
                                             << e.what() << ", cleaning up.";
                cleanupSharedResources(true); // Attempt cleanup as initializer
                throw; // Rethrow after cleanup attempt
            }
        } else if (errno == EEXIST) {
            // Region already exists, try to attach (subsequent process)
            db<SharedMemoryEngine>(INF) << "[SHM Engine] Shared memory region exists, attempting to attach.";
            try {
                attachToSharedMemory();
                // Need to wait until initializer finishes setting up
                // Spin-wait with a small delay, checking the initialized flag
                // TODO: Replace with a condition variable or dedicated semaphore if needed
                //       for extremely fast startup scenarios.
                timespec wait_time = {0, 1000000}; // 1ms
                int wait_count = 0;
                const int max_wait_count = 5000; // Wait up to 5 seconds
                while (!_shared_region->initialized.load(std::memory_order_acquire)) {
                    if (++wait_count > max_wait_count) {
                        throw std::runtime_error("Timeout waiting for SHM initializer");
                    }
                    nanosleep(&wait_time, nullptr);
                }
                db<SharedMemoryEngine>(INF) << "[SHM Engine] Initializer finished setup.";
                break; // Attachment successful
            } catch (const std::exception& e) {
                 db<SharedMemoryEngine>(ERR) << "[SHM Engine] Failed to attach to shared memory: "
                                              << e.what() << ", cleaning up.";
                 cleanupSharedResources(false); // Attempt cleanup as non-initializer
                 throw; // Rethrow after cleanup attempt
            }
        } else {
            // Another error occurred during shm_open
            perror("shm_open initial attempt");
             throw std::runtime_error("Failed to create or open shared memory");
        }
    }

    // --- Setup Timer FD for Polling --- 
    _poll_timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (_poll_timer_fd < 0) {
        perror("timerfd_create");
        cleanupSharedResources(_is_initializer);
        throw std::runtime_error("Failed to create poll timer FD");
    }

    struct itimerspec ts;
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = Traits<SharedMemoryEngine>::POLL_INTERVAL_MS * 1000000L; // Interval from traits (e.g., 10ms)
    ts.it_value.tv_sec = 0;
    ts.it_value.tv_nsec = Traits<SharedMemoryEngine>::POLL_INTERVAL_MS * 1000000L; // Initial expiration

    if (timerfd_settime(_poll_timer_fd, 0, &ts, NULL) < 0) {
        perror("timerfd_settime");
        close(_poll_timer_fd); _poll_timer_fd = -1;
        cleanupSharedResources(_is_initializer);
        throw std::runtime_error("Failed to set poll timer");
    }
    db<SharedMemoryEngine>(INF) << "[SHM Engine] Poll timer set with interval "
                               << Traits<SharedMemoryEngine>::POLL_INTERVAL_MS << " ms (fd=" << _poll_timer_fd << ").";


    _running.store(true, std::memory_order_release);
    db<SharedMemoryEngine>(INF) << "[SHM Engine] Started successfully.";
}

void SharedMemoryEngine::initializeSharedMemory() {
    // Set the size of the shared memory region
    if (ftruncate(_shm_fd, sizeof(SharedRegion)) == -1) {
        perror("ftruncate");
        throw std::runtime_error("Failed to set shared memory size");
    }

    // Map the shared memory region
    _shared_region = static_cast<SharedRegion*>(mmap(NULL, sizeof(SharedRegion), PROT_READ | PROT_WRITE, MAP_SHARED, _shm_fd, 0));
    if (_shared_region == MAP_FAILED) {
        perror("mmap initializer");
        _shared_region = nullptr; // Ensure pointer is null on failure
        throw std::runtime_error("Failed to map shared memory (initializer)");
    }
    db<SharedMemoryEngine>(INF) << "[SHM Engine] Shared memory mapped at " << _shared_region;

    // Initialize shared region metadata
    _shared_region->initialized.store(false, std::memory_order_relaxed); // Mark as not ready yet
    _shared_region->ref_count.store(1, std::memory_order_relaxed); // First process
    _shared_region->read_index = 0;
    _shared_region->write_index = 0;

    // Create and initialize named semaphores
    mode_t old_mask = umask(0); // Allow group permissions
    _mutex_sem = sem_open(SEM_MUTEX_NAME, O_CREAT | O_EXCL, 0660, 1); // Initial value 1 (unlocked)
    if (_mutex_sem == SEM_FAILED) { perror("sem_open mutex"); umask(old_mask); throw std::runtime_error("Failed to create mutex semaphore"); }

    _items_sem = sem_open(SEM_ITEMS_NAME, O_CREAT | O_EXCL, 0660, 0); // Initial value 0 (no items)
    if (_items_sem == SEM_FAILED) { perror("sem_open items"); sem_unlink(SEM_MUTEX_NAME); umask(old_mask); throw std::runtime_error("Failed to create items semaphore"); }

    _space_sem = sem_open(SEM_SPACE_NAME, O_CREAT | O_EXCL, 0660, SharedRegion::QUEUE_CAPACITY); // Initial value QUEUE_CAPACITY (all slots free)
    if (_space_sem == SEM_FAILED) { perror("sem_open space"); sem_unlink(SEM_MUTEX_NAME); sem_unlink(SEM_ITEMS_NAME); umask(old_mask); throw std::runtime_error("Failed to create space semaphore"); }
    umask(old_mask);
    db<SharedMemoryEngine>(INF) << "[SHM Engine] Named semaphores created and initialized.";

    // Mark initialization as complete *after* everything is set up
    _shared_region->initialized.store(true, std::memory_order_release);
    db<SharedMemoryEngine>(INF) << "[SHM Engine] Shared memory initialization complete.";
}

void SharedMemoryEngine::attachToSharedMemory() {
     // Open existing shared memory
    _shm_fd = shm_open(SHM_NAME, O_RDWR, 0660);
    if (_shm_fd < 0) {
        perror("shm_open attach");
        throw std::runtime_error("Failed to open existing shared memory");
    }

    // Map the existing shared memory region
    _shared_region = static_cast<SharedRegion*>(mmap(NULL, sizeof(SharedRegion), PROT_READ | PROT_WRITE, MAP_SHARED, _shm_fd, 0));
    if (_shared_region == MAP_FAILED) {
         perror("mmap attach");
         _shared_region = nullptr;
         close(_shm_fd); _shm_fd = -1;
         throw std::runtime_error("Failed to map shared memory (attach)");
    }
     db<SharedMemoryEngine>(INF) << "[SHM Engine] Shared memory mapped at " << _shared_region;

    // Open existing named semaphores
    _mutex_sem = sem_open(SEM_MUTEX_NAME, 0);
    if (_mutex_sem == SEM_FAILED) { perror("sem_open mutex attach"); throw std::runtime_error("Failed to open mutex semaphore"); }

    _items_sem = sem_open(SEM_ITEMS_NAME, 0);
    if (_items_sem == SEM_FAILED) { perror("sem_open items attach"); throw std::runtime_error("Failed to open items semaphore"); }

    _space_sem = sem_open(SEM_SPACE_NAME, 0);
    if (_space_sem == SEM_FAILED) { perror("sem_open space attach"); throw std::runtime_error("Failed to open space semaphore"); }
    db<SharedMemoryEngine>(INF) << "[SHM Engine] Named semaphores opened.";

    // Increment reference count atomically
    _shared_region->ref_count.fetch_add(1, std::memory_order_relaxed);
    db<SharedMemoryEngine>(INF) << "[SHM Engine] Attached. Ref count = " << _shared_region->ref_count.load();
}

void SharedMemoryEngine::cleanupSharedResources(bool was_initializer) {
     db<SharedMemoryEngine>(TRC) << "SharedMemoryEngine::cleanupSharedResources() called.";

    // Close Semaphores
    if (_mutex_sem != SEM_FAILED) { sem_close(_mutex_sem); _mutex_sem = SEM_FAILED; }
    if (_items_sem != SEM_FAILED) { sem_close(_items_sem); _items_sem = SEM_FAILED; }
    if (_space_sem != SEM_FAILED) { sem_close(_space_sem); _space_sem = SEM_FAILED; }

    // Unmap and close Shared Memory
    if (_shared_region != nullptr) {
        // Decrement reference count only if region was successfully mapped
        uint32_t previous_ref_count = _shared_region->ref_count.fetch_sub(1, std::memory_order_acq_rel);
        db<SharedMemoryEngine>(INF) << "[SHM Engine] Detached. Previous Ref count = " << previous_ref_count;

        bool should_unlink = (previous_ref_count == 1); // Unlink if we were the last one

        if (munmap(_shared_region, sizeof(SharedRegion)) == -1) {
             perror("munmap cleanup");
        }
        _shared_region = nullptr;

        if (_shm_fd >= 0) {
            close(_shm_fd);
            _shm_fd = -1;
        }

        // Unlink semaphores and SHM only if we are the last process
        if (should_unlink) {
             db<SharedMemoryEngine>(INF) << "[SHM Engine] Last process detached, unlinking resources.";
            if (sem_unlink(SEM_MUTEX_NAME) == -1 && errno != ENOENT) { perror("sem_unlink mutex"); }
            if (sem_unlink(SEM_ITEMS_NAME) == -1 && errno != ENOENT) { perror("sem_unlink items"); }
            if (sem_unlink(SEM_SPACE_NAME) == -1 && errno != ENOENT) { perror("sem_unlink space"); }
            if (shm_unlink(SHM_NAME) == -1 && errno != ENOENT) { perror("shm_unlink"); }
        }
    } else if (_shm_fd >= 0) {
        // If mapping failed but FD was opened (e.g., initializer failed early)
        close(_shm_fd);
        _shm_fd = -1;
        // Unlink if this instance was supposed to be the initializer
        if (was_initializer) {
            sem_unlink(SEM_MUTEX_NAME);
            sem_unlink(SEM_ITEMS_NAME);
            sem_unlink(SEM_SPACE_NAME);
            shm_unlink(SHM_NAME);
        }
    }

     // Close Timer FD
    if (_poll_timer_fd >= 0) {
        close(_poll_timer_fd);
        _poll_timer_fd = -1;
    }
     db<SharedMemoryEngine>(INF) << "[SHM Engine] Resource cleanup finished.";
}


void SharedMemoryEngine::stop() {
    db<SharedMemoryEngine>(TRC) << "SharedMemoryEngine::stop() called!\n";
    if (!running()) {
        db<SharedMemoryEngine>(INF) << "[SHM Engine] stop() called but not running.";
        return;
    }
    _running.store(false, std::memory_order_release);

    cleanupSharedResources(_is_initializer);
    db<SharedMemoryEngine>(INF) << "[SHM Engine] Stopped.";
}

const bool SharedMemoryEngine::running() const {
    return _running.load(std::memory_order_acquire);
}

int SharedMemoryEngine::send(Ethernet::Frame* frame, unsigned int size) {
    db<SharedMemoryEngine>(TRC) << "SharedMemoryEngine::send() called!\n";
    
    if (!running() || !_shared_region) {
        db<SharedMemoryEngine>(WRN) << "[SHM Engine] Attempted send while stopped or not initialized.\n";
        return -1;
    }

    if (!frame) {
        db<SharedMemoryEngine>(ERR) << "[SHM Engine] Send called with null frame.\n";
        return -1;
    }
    
    unsigned int payload_size = size - Ethernet::HEADER_SIZE;
    if (payload_size > Traits<SharedMemoryEngine>::MTU) {
         db<SharedMemoryEngine>(ERR) << "[SHM Engine] Send payload size (" << payload_size
                                      << ") exceeds MTU (" << Traits<SharedMemoryEngine>::MTU << ").\n";
         return -1;
    }

    // 1. Wait for space
    db<SharedMemoryEngine>(TRC) << "[SHM Engine] Waiting for space semaphore...\n";
    if (sem_wait(_space_sem) == -1) {
        perror("sem_wait space");
        return -1; // Error waiting
    }
    db<SharedMemoryEngine>(TRC) << "[SHM Engine] Space semaphore acquired.\n";

    // Re-check running status after potentially blocking
    if (!running()) {
         db<SharedMemoryEngine>(WRN) << "[SHM Engine] Engine stopped while waiting for space semaphore.";
         sem_post(_space_sem); // Release the space count we acquired
         return -1;
    }

    // 2. Acquire mutex
    db<SharedMemoryEngine>(TRC) << "[SHM Engine] Waiting for mutex semaphore...\n";
    if (sem_wait(_mutex_sem) == -1) {
        perror("sem_wait mutex");
        sem_post(_space_sem); // Release the space count
        return -1;
    }
     db<SharedMemoryEngine>(TRC) << "[SHM Engine] Mutex semaphore acquired.\n";

    // --- CRITICAL SECTION --- 
    int bytes_written = 0;
    try {
        uint32_t write_idx = _shared_region->write_index;
        SharedFrameData* target_slot = &(_shared_region->buffer[write_idx]);

        // Copy data
        target_slot->protocol = frame->prot; // Assume host order
        target_slot->payload_size = payload_size;
        std::memcpy(target_slot->payload, frame->payload, payload_size);

        // Update write index
        _shared_region->write_index = (write_idx + 1) % SharedRegion::QUEUE_CAPACITY;

        bytes_written = payload_size; // Indicate success by returning payload size
         db<SharedMemoryEngine>(INF) << "[SHM Engine] Frame written to slot " << write_idx
                                     << " {proto=" << target_slot->protocol
                                     << ", payload_size=" << target_slot->payload_size << "}";

    } catch (const std::exception& e) {
        db<SharedMemoryEngine>(ERR) << "[SHM Engine] Exception during write critical section: " << e.what();
        bytes_written = -1; // Indicate error
    }
    // --- END CRITICAL SECTION ---

    // 3. Release mutex
    if (sem_post(_mutex_sem) == -1) {
        perror("sem_post mutex");
        // Log error, but continue to post items semaphore if write succeeded?
        // Might lead to inconsistent state.
    }
    db<SharedMemoryEngine>(TRC) << "[SHM Engine] Mutex semaphore released.";

    // 4. Signal item availability (only if write was successful)
    if (bytes_written >= 0) {
        if (sem_post(_items_sem) == -1) {
            perror("sem_post items");
            // This is problematic, item was added but couldn't signal.
            // May need recovery logic or log severe error.
            return -1; // Indicate failure
        }
         db<SharedMemoryEngine>(TRC) << "[SHM Engine] Items semaphore posted.";
    } else {
        // If write failed, we need to release the space semaphore we acquired earlier
        sem_post(_space_sem);
        db<SharedMemoryEngine>(WRN) << "[SHM Engine] Write failed, releasing space semaphore.";
    }

    return bytes_written; // Return payload size on success, -1 on error
}

int SharedMemoryEngine::receiveData(void* payload_buf, unsigned int max_size, Ethernet::Protocol* proto_out) {
    db<SharedMemoryEngine>(TRC) << "SharedMemoryEngine::receiveData() called!\n";

    if (!running() || !_shared_region) {
         db<SharedMemoryEngine>(WRN) << "[SHM Engine] Attempted receive while stopped or not initialized.\n";
         return -1;
    }

    if (!payload_buf || !proto_out) {
         db<SharedMemoryEngine>(ERR) << "[SHM Engine] Receive called with null output pointers.\n";
         return -1;
    }

    // 1. Try to acquire an item without blocking indefinitely
    db<SharedMemoryEngine>(TRC) << "[SHM Engine] Trying items semaphore...\n";
    int wait_result = sem_trywait(_items_sem);

    if (wait_result == -1) {
        if (errno == EAGAIN) {
             db<SharedMemoryEngine>(TRC) << "[SHM Engine] No items currently available (sem_trywait EAGAIN).\n";
             return 0; // No data available right now
        } else {
            perror("sem_trywait items");
            return -1; // Other semaphore error
        }
    }
     db<SharedMemoryEngine>(TRC) << "[SHM Engine] Items semaphore acquired.\n";

    // Re-check running status after potentially blocking (though trywait shouldn't)
    if (!running()) {
         db<SharedMemoryEngine>(WRN) << "[SHM Engine] Engine stopped after acquiring items semaphore.";
         sem_post(_items_sem); // Release the item count
         return -1;
    }

    // 2. Acquire mutex
    db<SharedMemoryEngine>(TRC) << "[SHM Engine] Waiting for mutex semaphore...\n";
    if (sem_wait(_mutex_sem) == -1) {
        perror("sem_wait mutex receive");
        sem_post(_items_sem); // Release the item count we acquired
        return -1;
    }
    db<SharedMemoryEngine>(TRC) << "[SHM Engine] Mutex semaphore acquired.";

    // --- CRITICAL SECTION --- 
    int bytes_read = 0;
    try {
        uint32_t read_idx = _shared_region->read_index;
        SharedFrameData* source_slot = &(_shared_region->buffer[read_idx]);

        db<SharedMemoryEngine>(INF) << "[SHM Engine] Reading from slot " << read_idx
                                     << " {proto=" << source_slot->protocol
                                     << ", payload_size=" << source_slot->payload_size << "}";

        // Check buffer size
        if (source_slot->payload_size > max_size) {
            db<SharedMemoryEngine>(ERR) << "[SHM Engine] Receive buffer too small (" << max_size
                                         << " bytes) for payload (" << source_slot->payload_size << " bytes). Dropping item.";
            bytes_read = -2; // Indicate error: buffer too small
        } else {
            // Copy data
            *proto_out = source_slot->protocol;
            std::memcpy(payload_buf, source_slot->payload, source_slot->payload_size);
            bytes_read = source_slot->payload_size;
        }

        // Update read index (even if buffer was too small, consume the slot)
        _shared_region->read_index = (read_idx + 1) % SharedRegion::QUEUE_CAPACITY;

    } catch (const std::exception& e) {
        db<SharedMemoryEngine>(ERR) << "[SHM Engine] Exception during read critical section: " << e.what();
        bytes_read = -1; // Indicate general error
    }
    // --- END CRITICAL SECTION ---

    // 3. Release mutex
    if (sem_post(_mutex_sem) == -1) {
        perror("sem_post mutex receive");
        // Error releasing mutex, state might be inconsistent.
    }
    db<SharedMemoryEngine>(TRC) << "[SHM Engine] Mutex semaphore released.";

    // 4. Signal space availability (only if item was successfully consumed/dequeued)
    // We consumed an item slot regardless of whether the user buffer was large enough
    if (bytes_read >= 0 || bytes_read == -2) {
        if (sem_post(_space_sem) == -1) {
            perror("sem_post space");
            // Error signaling space, potential deadlock later.
        }
         db<SharedMemoryEngine>(TRC) << "[SHM Engine] Space semaphore posted.";
    } else {
        // If read failed due to other errors, we need to release the item semaphore count
        sem_post(_items_sem);
         db<SharedMemoryEngine>(WRN) << "[SHM Engine] Read failed, releasing items semaphore.";
    }

    return (bytes_read >= 0) ? bytes_read : -1; // Return payload size or -1 for errors (-2 was handled internally)
}

int SharedMemoryEngine::getNotificationFd() const {
    db<SharedMemoryEngine>(TRC) << "SharedMemoryEngine::getNotificationFd() called.\n";
    return _poll_timer_fd; // Return the timer FD for polling
}

Ethernet::Address SharedMemoryEngine::getMacAddress() const {
    db<SharedMemoryEngine>(TRC) << "SharedMemoryEngine::getMacAddress() called.\n";
    return Ethernet::NULL_ADDRESS; // Internal engine doesn't have a specific external MAC
}

#endif // SHAREDMEMORYENGINE_H 