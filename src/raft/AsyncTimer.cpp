#include "raft/AsyncTimer.hpp"

namespace raft {

AsyncTimer::AsyncTimer() : running(false) {}

void AsyncTimer::start(std::function<std::chrono::milliseconds()> intervalProvider, std::function<void()> callback) {
    stop();
    running = true;
    worker = std::thread([this, intervalProvider, callback]() {
        std::unique_lock<std::mutex> lock(mtx);
        while (running) {
            auto interval = intervalProvider();
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
