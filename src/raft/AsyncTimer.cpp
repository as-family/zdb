#include "raft/AsyncTimer.hpp"
#include <iostream>

namespace raft {

AsyncTimer::AsyncTimer() : running(false), mtx{}, cv{} {}

void AsyncTimer::start(std::function<std::chrono::milliseconds()> intervalProvider, std::function<void()> callback) {
    stop();
    running = true;
    worker = std::thread([this, intervalProvider, callback]() {
        while (running) {
            auto interval = intervalProvider();
            std::unique_lock<std::mutex> lock(mtx);
            if (cv.wait_for(lock, interval, [this]{ return !running; })) break;
            if (running) callback();
        }
    });
}

void AsyncTimer::stop() {
    {
        std::lock_guard<std::mutex> lock(mtx);
        running = false;
    }
    cv.notify_all();
    if (worker.joinable()) worker.join();
}

AsyncTimer::~AsyncTimer() {
    stop();
}
} // namespace raft
