#include "observer.h"

// Semaphore implementation
Semaphore::Semaphore(int count) : _count(count) {}

void Semaphore::p() {
    std::unique_lock<std::mutex> lock(_mutex);
    while (_count == 0) {
        _cv.wait(lock);
    }
    _count--;
}

void Semaphore::v() {
    std::unique_lock<std::mutex> lock(_mutex);
    _count++;
    _cv.notify_one();
} 