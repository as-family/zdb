#include "raft/Channel.hpp"

namespace raft {

Channel::Channel() {}

Channel::~Channel() {}

void Channel::send(Command* cmd) {
    std::unique_lock<std::mutex> lock(m);
    queue.push(cmd);
    cv.notify_one();
}

Command* Channel::receive() {
    std::unique_lock<std::mutex> lock(m);
    while (queue.empty()) {
        cv.wait(lock);
    }
    Command* cmd = queue.front();
    queue.pop();
    return cmd;
}

} // namespace raft
