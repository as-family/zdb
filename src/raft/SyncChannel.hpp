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
#ifndef RAFT_SYNC_CHANNEL_H
#define RAFT_SYNC_CHANNEL_H

#include "raft/Command.hpp"
#include <mutex>
#include <optional>
#include <condition_variable>
#include "raft/Channel.hpp"

namespace raft {

template<typename T>
class SyncChannel : public Channel<T> {
public:
    void send(T) override;
    bool sendUntil(T, std::chrono::system_clock::time_point t) override;
    std::optional<T> receive() override;
    std::optional<T> receiveUntil(std::chrono::system_clock::time_point t) override;
    virtual void close() override;
    virtual bool isClosed() override;
    ~SyncChannel() override;
private:
    void doClose() noexcept;
    std::mutex m;
    std::condition_variable cv;
    std::optional<T> value;
    bool closed = false;
};

template<typename T>
void SyncChannel<T>::send(T cmd) {
    std::unique_lock<std::mutex> lock(m);
    while(value.has_value() && !closed) {
        cv.wait(lock);
    }
    if (closed) {
        return;
    }
    value = std::move(cmd);
    cv.notify_one();
}

template<typename T>
bool SyncChannel<T>::sendUntil(T cmd, std::chrono::system_clock::time_point t) {
    std::unique_lock<std::mutex> lock(m);
    while (value.has_value() && !closed) {
        if (cv.wait_until(lock, t) == std::cv_status::timeout) {
            return false;
        }
    }
    if (closed) {
        return false;
    }
    value = std::move(cmd);
    cv.notify_one();
    return true;
}

template<typename T>
std::optional<T> SyncChannel<T>::receive() {
    std::unique_lock<std::mutex> lock(m);
    while (!value.has_value() && !closed) {
        cv.wait(lock);
    }
    if (value.has_value()) {
        T cmd = std::move(*value);
        value.reset();
        cv.notify_one();
        return cmd;
    }
    // closed and no value available
    return std::nullopt;
}

template<typename T>
std::optional<T> SyncChannel<T>::receiveUntil(std::chrono::system_clock::time_point t) {
    std::unique_lock<std::mutex> lock(m);
    while (!value.has_value() && !closed) {
        if (cv.wait_until(lock, t) == std::cv_status::timeout) {
            return std::nullopt;
        }
    }
    if (value.has_value()) {
        T cmd = std::move(*value);
        value.reset();
        cv.notify_one();
        return cmd;
    }
    // closed and no value available
    return std::nullopt;
}

template<typename T>
void SyncChannel<T>::doClose() noexcept {
    std::unique_lock<std::mutex> lock(m);
    closed = true;
    cv.notify_all();
}

template<typename T>
void SyncChannel<T>::close() {
    doClose();
}

template<typename T>
bool SyncChannel<T>::isClosed() {
    std::unique_lock<std::mutex> lock(m);
    return closed;
}

template<typename T>
SyncChannel<T>::~SyncChannel() {
    doClose();
}

} // namespace raft

#endif // RAFT_SYNC_CHANNEL_H
