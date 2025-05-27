#ifndef PERIODIC_THREAD
#define PERIODIC_THREAD

#include <pthread.h>
#include <chrono>
#include <atomic>
#include <thread>
#include <functional>
#include <cstdint>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <cstring>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/sched.h>
#include <errno.h>
#include <signal.h>  // For signal handling

// Definition of sched_attr structure
struct sched_attr {
    uint32_t size;
    uint32_t sched_policy;
    uint64_t sched_flags;
    int32_t  sched_nice;
    uint32_t sched_priority;
    // SCHED_DEADLINE specific fields
    uint64_t sched_runtime;
    uint64_t sched_deadline;
    uint64_t sched_period;
};

// Signal handler for thread interruption
// Use a single static handler for the entire component system
extern "C" void component_signal_handler(int sig) {
    // Simply wake up the thread to check its running state
    if (sig == SIGUSR1) {
        // No action needed, just unblock from system calls
    }
}

// Wrapper for the sched_setattr system call
int sched_setattr(pid_t pid, const struct sched_attr *attr, unsigned int flags) {
    return syscall(SYS_sched_setattr, pid, attr, flags);
}

template <typename Owner>
class Periodic_Thread {
    public:
        Periodic_Thread() = default;
        template <typename ...Tn>
        Periodic_Thread(Owner* owner, void (Owner::*task)(Tn...), Tn...an);
        ~Periodic_Thread();

        void start(std::int64_t period);
        void join();

        void adjust_period(std::int64_t period);
        std::int64_t period() const;
        
        static void* run(void* arg);
        bool running();

        Periodic_Thread(const Periodic_Thread&) = delete;
        Periodic_Thread& operator=(const Periodic_Thread&) = delete;

    private:
        std::int64_t mdc(std::int64_t a, std::int64_t b);

        std::atomic<std::int64_t> _period;
        std::atomic<bool> _running;
        pthread_t _thread;
        std::function<void()> _task;
};

/***** Periodic Thread Implementation *****/
template <typename Owner>
template <typename ...Tn>
Periodic_Thread<Owner>::Periodic_Thread(Owner* owner, void (Owner::*task)(Tn...), Tn...an) : _running(false), _thread(0) {
    _task = std::bind(task, owner, std::forward<Tn>(an)...);
    _period.store(0, std::memory_order_release);
}

template <typename Owner>
Periodic_Thread<Owner>::~Periodic_Thread() {
    join();
}

template <typename Owner>
void Periodic_Thread<Owner>::join() {
    if (running()) {
        _running.store(false, std::memory_order_release);
        // Send a signal to interrupt any blocked thread (critical for proper thread termination)
        if (_thread != 0) {
            pthread_kill(_thread, SIGUSR1);
            // Actually join the thread
            pthread_join(_thread, nullptr);
            _thread = 0;
        }
    }
}

template <typename Owner>
void Periodic_Thread<Owner>::start(std::int64_t period) {
    if (!running()) {
        _period.store(period, std::memory_order_release);
        _running.store(true, std::memory_order_release);
        // Fix: pthread_create returns error code, not thread ID
        int result = pthread_create(&_thread, nullptr, Periodic_Thread::run, this);
        if (result != 0) {
            _running.store(false, std::memory_order_release);
            throw std::runtime_error("Failed to create periodic thread");
        }
    }
}

template <typename Owner>
void Periodic_Thread<Owner>::adjust_period(std::int64_t period) {
    _period.store(mdc(_period.load(std::memory_order_acquire), period), std::memory_order_release);
}

template <typename Owner>
std::int64_t Periodic_Thread<Owner>::period() const {
    return _period.load(std::memory_order_acquire);
}

template <typename Owner>
void* Periodic_Thread<Owner>::run(void* arg) {
    Periodic_Thread* thread = static_cast<Periodic_Thread*>(arg);

    // Install the signal handler for thread interruption
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = component_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, nullptr);

    struct sched_attr attr_dl;
    memset(&attr_dl, 0, sizeof(attr_dl));
    attr_dl.size = sizeof(attr_dl);
    attr_dl.sched_policy = SCHED_DEADLINE;
    attr_dl.sched_flags = 0;

    while (thread->running()) {
        uint64_t current_period = thread->period();
        // Update SCHED_DEADLINE parameters based on current period
        attr_dl.sched_runtime = current_period * 500; // 50% of period in ns
        attr_dl.sched_deadline = current_period * 1000; // Period in ns
        attr_dl.sched_period = current_period * 1000; // Period in ns
        int result = sched_setattr(0, &attr_dl, 0);
        if (result != 0) {
            std::cerr << "Error setting SCHED_DEADLINE: " << strerror(errno) << std::endl;
            return nullptr;
        }

        // Double-check running status before calling task
        if (thread->running()) {
            thread->_task();
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(thread->period()));
    }

    return nullptr;
}

template <typename Owner>
bool Periodic_Thread<Owner>::running() {
    return _running.load(std::memory_order_acquire);
}

template <typename Owner>
std::int64_t Periodic_Thread<Owner>::mdc(std::int64_t a, std::int64_t b) {
    while (b != 0) {
        std::int64_t temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

#endif // PERIODIC_THREAD