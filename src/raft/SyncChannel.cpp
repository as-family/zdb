#include "raft/SyncChannel.hpp"

namespace raft {

void SyncChannel::send(std::string cmd) {
    std::unique_lock<std::mutex> lock(m);
    while(value.has_value() && !value.value().empty()) {
        cv.wait(lock);
    }
    if (value.has_value()) {
        return;
    }
    value = cmd;
    cv.notify_one();
}

std::string SyncChannel::receive() {
    std::unique_lock<std::mutex> lock(m);
    while (!value.has_value()) {
        cv.wait(lock);
    }
    std::string cmd = value.value();
    if (cmd == "") {
        return cmd;
    }
    value.reset();
    cv.notify_one();
    return cmd;
}

SyncChannel::~SyncChannel() {
    std::unique_lock<std::mutex> lock(m);
    value = "";
    cv.notify_all();
    lock.unlock();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

} // namespace raft
