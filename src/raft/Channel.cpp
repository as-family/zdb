#include "raft/Channel.hpp"

namespace raft {

Channel::Channel() {}

Channel::~Channel() {}

void Channel::send(Command* cmd) {
    std::lock_guard<std::mutex> lock(m);
    queue.push(cmd);
}

Command* Channel::receive() {
    std::lock_guard<std::mutex> lock(m);
    if (queue.empty()) {
        return nullptr;
    }
    Command* cmd = queue.front();
    queue.pop();
    return cmd;
}

} // namespace raft
