#ifndef RAFT_CHANNEL_H
#define RAFT_CHANNEL_H

#include "raft/Command.hpp"
#include <mutex>
#include <queue>
#include <condition_variable>

namespace raft {

class Channel {
public:
    virtual ~Channel() = default;
    virtual void send(std::string) = 0;
    virtual std::string receive() = 0;
};

} // namespace raft

#endif // RAFT_CHANNEL_H
