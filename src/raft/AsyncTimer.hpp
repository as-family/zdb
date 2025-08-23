#ifndef ASYNC_TIMER_H
#define ASYNC_TIMER_H

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include <condition_variable>

namespace raft {

class AsyncTimer {
public:
    AsyncTimer();
    void start(std::function<std::chrono::milliseconds()> intervalProvider, std::function<void()> callback);
    void stop();
    ~AsyncTimer();
private:
    std::atomic<bool> running;
    std::thread worker;
    std::mutex mtx;
    std::condition_variable cv;
};

} // namespace raft

#endif // ASYNC_TIMER_H
