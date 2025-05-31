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
#include <signal.h>  // For signal handling

// Definition of sched_attr structure (only if not already defined)
// Robust __has_include detection for cross-platform compatibility
#ifdef __has_include
  // Compiler supports __has_include builtin
  #if __has_include(<linux/sched/types.h>)
    #include <linux/sched/types.h>
    #define __SCHED_ATTR_AVAILABLE__ 1
  #endif
#else
  // Compiler doesn't support __has_include, define fallback
  #define __has_include(x) 0
#endif

#if !defined(__SCHED_ATTR_AVAILABLE__) && !defined(__SCHED_ATTR_SIZE__)
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
#else
#include <linux/sched/types.h>
#endif

// Signal handler for thread interruption
// Use a single static handler for the entire component system
extern "C" inline void component_signal_handler(int sig) {
    // Simply wake up the thread to check its running state
    if (sig == SIGUSR1) {
        // No action needed, just unblock from system calls
    }
}

// Wrapper for the sched_setattr system call (only if not already available)
// #ifndef SYS_sched_setattr
// #define SYS_sched_setattr 314
// #endif

#if !defined(HAVE_SCHED_SETATTR)
int sched_setattr(pid_t pid, const struct sched_attr *attr, unsigned int flags) {
    return syscall(SYS_sched_setattr, pid, attr, flags);
}
#endif

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

    // SCHED_DEADLINE limits (conservative estimates for portability)
    static constexpr uint64_t MAX_DEADLINE_PERIOD_US = 1000000ULL;    // 1 second in microseconds
    
    bool use_deadline_scheduling = false;
    struct sched_attr attr_dl;
    
    while (thread->running()) {
        uint64_t current_period_us = thread->period();
        
        // Determine if we should use SCHED_DEADLINE based on period length
        bool should_use_deadline = (current_period_us <= MAX_DEADLINE_PERIOD_US);
        
        // Only attempt SCHED_DEADLINE for periods <= 1 second
        if (should_use_deadline && !use_deadline_scheduling) {
            memset(&attr_dl, 0, sizeof(attr_dl));
            attr_dl.size = sizeof(attr_dl);
            attr_dl.sched_policy = SCHED_DEADLINE;
            attr_dl.sched_flags = 0;
            
            // Convert microseconds to nanoseconds for SCHED_DEADLINE
            uint64_t period_ns = current_period_us * 1000;
            attr_dl.sched_runtime = period_ns / 2;  // 50% of period for execution
            attr_dl.sched_deadline = period_ns;     // Deadline equals period
            attr_dl.sched_period = period_ns;       // Period in nanoseconds
            
            int result = sched_setattr(0, &attr_dl, 0);
            if (result == 0) {
                use_deadline_scheduling = true;
            } else {
                use_deadline_scheduling = false;
            }
        } else if (!should_use_deadline && use_deadline_scheduling) {
            // Switch back to regular scheduling for long periods
            struct sched_param param;
            param.sched_priority = 0;
            int result = pthread_setschedparam(pthread_self(), SCHED_OTHER, &param);
            if (result == 0) {
                use_deadline_scheduling = false;
            }
        }

        // Double-check running status before calling task
        if (thread->running()) {
            thread->_task();
        }
        
        // Sleep for the specified period (convert microseconds to milliseconds)
        std::this_thread::sleep_for(std::chrono::microseconds(current_period_us));
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