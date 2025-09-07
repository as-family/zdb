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
#include "raft/SyncChannel.hpp"

namespace raft {

void SyncChannel::send(std::string cmd) {
    std::unique_lock<std::mutex> lock(m);
    while(value.has_value() && !closed) {
        cv.wait(lock);
    }
    if (closed) {
        return;
    }
    value = cmd;
    cv.notify_one();
}

bool SyncChannel::sendUntil(std::string cmd, std::chrono::system_clock::time_point t) {
    std::unique_lock<std::mutex> lock(m);
    while (value.has_value() && !closed) {
        if (cv.wait_until(lock, t) == std::cv_status::timeout) {
            return false;
        }
    }
    if (closed) {
        return false;
    }
    value = cmd;
    cv.notify_one();
    return true;
}

std::string SyncChannel::receive() {
    std::unique_lock<std::mutex> lock(m);
    while (!value.has_value() && !closed) {
        cv.wait(lock);
    }
    if (value.has_value()) {
        std::string cmd = std::move(*value);
        value.reset();
        cv.notify_one();
        return cmd;
    }
    // closed and no value available
    return "";
}

std::optional<std::string> SyncChannel::receiveUntil(std::chrono::system_clock::time_point t) {
    std::unique_lock<std::mutex> lock(m);
    while (!value.has_value() && !closed) {
        if (cv.wait_until(lock, t) == std::cv_status::timeout) {
            return std::nullopt;
        }
    }
    if (value.has_value()) {
        std::string cmd = std::move(*value);
        value.reset();
        cv.notify_one();
        return cmd;
    }
    // closed and no value available
    return std::nullopt;
}

void SyncChannel::doClose() noexcept {
    std::unique_lock<std::mutex> lock(m);
    closed = true;
    cv.notify_all();
}

void SyncChannel::close() {
    doClose();
}

bool SyncChannel::isClosed() {
    std::unique_lock<std::mutex> lock(m);
    return closed;
}

SyncChannel::~SyncChannel() {
    doClose();
}

} // namespace raft
