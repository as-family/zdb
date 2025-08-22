#ifndef RAFT_CHANNEL_H
#define RAFT_CHANNEL_H

#include "raft/Command.hpp"
#include <mutex>
#include <queue>

namespace raft {

class Channel {
public:
    Channel();
    ~Channel();
    void send(Command* cmd);
    Command* receive();
private:
    std::mutex m;
    std::queue<Command*> queue;
};

} // namespace raft

#endif // RAFT_CHANNEL_H
