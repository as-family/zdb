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

void SyncChannel::close() {
    std::unique_lock<std::mutex> lock(m);
    closed = true;
    cv.notify_all();
}

bool SyncChannel::isClosed() {
    std::unique_lock<std::mutex> lock(m);
    return closed;
}

SyncChannel::~SyncChannel() {
    close();
}

} // namespace raft
