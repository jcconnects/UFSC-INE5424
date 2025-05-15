#ifndef PERIODIC_THREAD
#define PERIODIC_THREAD

#include <pthread.h>
#include <chrono>
#include <atomic>
#include <thread>
#include <vector>
#include "agent.h"

class Periodic_Thread {
    public:
        Periodic_Thread(Agent* owner);
        ~Periodic_Thread();

        void start(std::chrono::microseconds period);
        void stop();

        void set_period(std::chrono::microseconds period);
        const std::chrono::microseconds period();
        
        static void* run(void* arg);
        bool running();
        
        Agent* owner();

    private:
        std::int64_t mdc(std::int64_t a, std::int64_t b);

        std::chrono::microseconds _period;
        std::atomic<bool> _running;
        pthread_t _thread;
        Agent* _owner;
};

/***** Periodic Thread Implementation *****/
Periodic_Thread::Periodic_Thread(Agent* owner) : _owner(owner), _period(std::chrono::microseconds::zero()), _running(false), _thread(0) {
}

Periodic_Thread::~Periodic_Thread() {
    stop();
}

void Periodic_Thread::start(std::chrono::microseconds period) {
    if (!running()) {
        _period = period;
        _running.store(true, std::memory_order_release);
        pthread_create(&_thread, nullptr, Periodic_Thread::run, this);
    }
}

void Periodic_Thread::stop() {
    _running.store(false, std::memory_order_release);
}

void Periodic_Thread::set_period(std::chrono::microseconds period) {
    _period = std::chrono::microseconds(mdc(_period.count(), period.count()));
}

const std::chrono::microseconds Periodic_Thread::period() {
    return _period;
}

void* Periodic_Thread::run(void* arg) {
    Periodic_Thread* thread = static_cast<Periodic_Thread*>(arg);

    while (thread->running()) {
        std::vector<std::uint8_t> value = thread->owner()->get(thread->owner()->type());
        thread->owner()->send(Message::Type::RESPONSE, thread->owner()->type(), std::chrono::microseconds::zero(), static_cast<void*>(value.data()), value.size());
        std::this_thread::sleep_for(thread->period());
    }
}

bool Periodic_Thread::running() {
    return _running.load(std::memory_order_acquire);
}

Agent* Periodic_Thread::owner() {
    return _owner;
}

#endif // PERIODIC_THREAD