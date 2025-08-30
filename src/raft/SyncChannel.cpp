#include "raft/SyncChannel.hpp"

namespace raft {

void SyncChannel::send(std::string cmd) {
    std::unique_lock<std::mutex> lock(m);
    while(value.has_value()) {
        cv.wait(lock);
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
    value.reset();
    return cmd;
}

} // namespace raft
