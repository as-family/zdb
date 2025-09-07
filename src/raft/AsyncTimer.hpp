/*
 * ZDB a distributed, fault-tolerant database.
 * Copyright (C) 2025 Ahmed Refaat Gadalla Mohamed
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef ASYNC_TIMER_H
#define ASYNC_TIMER_H

#include <mutex>
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
    AsyncTimer(const AsyncTimer&) = delete;
    AsyncTimer& operator=(const AsyncTimer&) = delete;
    AsyncTimer(AsyncTimer&&) = delete;
    AsyncTimer& operator=(AsyncTimer&&) = delete;
private:
    std::atomic<bool> running;
    std::thread worker;
    std::mutex mtx;
    std::condition_variable cv;
};

} // namespace raft

#endif // ASYNC_TIMER_H
