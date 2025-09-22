// SPDX-License-Identifier: AGPL-3.0-or-later
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
#include "raft/AsyncTimer.hpp"
#include <thread>
#include <chrono>
#include <functional>
#include <mutex>

namespace raft {

AsyncTimer::AsyncTimer() : running(false), mtx{}, cv{} {}

void AsyncTimer::start(std::function<std::chrono::milliseconds()> intervalProvider, std::function<void()> callback) {
    {
        std::lock_guard<std::mutex> lock(mtx);
        running = true;
    }
    worker = std::thread([this, intervalProvider, callback]() {
        while (true) {
            std::unique_lock<std::mutex> lock(mtx);
            if (!running) break;
            auto interval = intervalProvider();
            if (cv.wait_for(lock, interval, [this]{ return !running; })) break;
            lock.unlock();
            callback();
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
