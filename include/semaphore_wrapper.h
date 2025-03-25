#ifndef SEMAPHORE_WRAPPER_H
#define SEMAPHORE_WRAPPER_H

#include <semaphore.h>
#include <atomic>
#include <stdexcept>
#include <unistd.h>  // for getpid()
#include <fcntl.h>   // for O_CREAT, O_EXCL
#include <cstdio>    // for snprintf

// Cross-platform semaphore wrapper
class SemaphoreWrapper {
private:
#ifdef __APPLE__
    sem_t* _sem;
    char _name[32];
    static std::atomic<int> _sem_count;
#else
    sem_t _sem;
#endif

public:
    SemaphoreWrapper(int initial_value = 0) {
#ifdef __APPLE__
        // Generate unique name for the semaphore
        snprintf(_name, sizeof(_name), "/sem_%d_%d", getpid(), ++_sem_count);
        _sem = sem_open(_name, O_CREAT | O_EXCL, 0644, initial_value);
        if (_sem == SEM_FAILED) {
            throw std::runtime_error("Failed to create semaphore");
        }
#else
        if (sem_init(&_sem, 0, initial_value) != 0) {
            throw std::runtime_error("Failed to create semaphore");
        }
#endif
    }

    ~SemaphoreWrapper() {
#ifdef __APPLE__
        sem_close(_sem);
        sem_unlink(_name);
#else
        sem_destroy(&_sem);
#endif
    }

    void post() {
#ifdef __APPLE__
        sem_post(_sem);
#else
        sem_post(&_sem);
#endif
    }

    void wait() {
#ifdef __APPLE__
        sem_wait(_sem);
#else
        sem_wait(&_sem);
#endif
    }

    // Deleted copy constructor and assignment operator to prevent multiple instances
    // from managing the same semaphore
    SemaphoreWrapper(const SemaphoreWrapper&) = delete;
    SemaphoreWrapper& operator=(const SemaphoreWrapper&) = delete;
};

#ifdef __APPLE__
std::atomic<int> SemaphoreWrapper::_sem_count(0);
#endif

#endif // SEMAPHORE_WRAPPER_H 