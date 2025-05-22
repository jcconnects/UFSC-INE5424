#ifndef PERIODIC_THREAD
#define PERIODIC_THREAD

#include <pthread.h>
#include <chrono>
#include <atomic>
#include <thread>
#include <functional>
#include <condition_variable>
#include <mutex>


template <typename Owner>
class Periodic_Thread {
    public:
        Periodic_Thread() = default;
        template <typename ...Tn>
        Periodic_Thread(Owner* owner, void (Owner::*task)(Tn...), Tn...an);
        ~Periodic_Thread();

        void start(std::chrono::microseconds period);
        void join();

        void adjust_period(std::chrono::microseconds period);
        std::chrono::microseconds period() const;
        
        static void* run(void* arg);
        bool running();

        Periodic_Thread(const Periodic_Thread&) = delete;
        Periodic_Thread& operator=(const Periodic_Thread&) = delete;

    private:
        std::int64_t mdc(std::int64_t a, std::int64_t b);

        std::chrono::microseconds _period;
        std::atomic<bool> _running;
        pthread_t _thread;
        std::condition_variable _cv;
        std::function<void()> _task;
        std::mutex _mutex;
};

/***** Periodic Thread Implementation *****/
template <typename Owner>
template <typename ...Tn>
Periodic_Thread<Owner>::Periodic_Thread(Owner* owner, void (Owner::*task)(Tn...), Tn...an) : _period(std::chrono::microseconds::zero()), _running(false), _thread(0) {
    _task = std::bind(task, owner, std::forward<Tn>(an)...);
}

template <typename Owner>
Periodic_Thread<Owner>::~Periodic_Thread() {
    join();
}

template <typename Owner>
void Periodic_Thread<Owner>::join() {
    if (running()) {
        _running.store(false, std::memory_order_release);
        _cv.notify_all();
        pthread_join(_thread, nullptr);
    }
}

template <typename Owner>
void Periodic_Thread<Owner>::start(std::chrono::microseconds period) {
    if (!running()) {
        _period = period;
        _running.store(true, std::memory_order_release);
        pthread_create(&_thread, nullptr, Periodic_Thread::run, this);
    }
}

template <typename Owner>
void Periodic_Thread<Owner>::adjust_period(std::chrono::microseconds period) {
    _period = std::chrono::microseconds(mdc(_period.count(), period.count()));
}

template <typename Owner>
std::chrono::microseconds Periodic_Thread<Owner>::period() const {
    return _period;
}

template <typename Owner>
void* Periodic_Thread<Owner>::run(void* arg) {
    Periodic_Thread* thread = static_cast<Periodic_Thread*>(arg);

    while (thread->running()) {
        thread->_task();
        std::this_thread::sleep_for(thread->period());
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