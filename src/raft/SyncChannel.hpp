#ifndef RAFT_SYNC_CHANNEL_H
#define RAFT_SYNC_CHANNEL_H

#include "raft/Command.hpp"
#include <mutex>
#include <optional>
#include <condition_variable>
#include "raft/Channel.hpp"

namespace raft {

class SyncChannel : public Channel {
public:
    void send(std::string) override;
    std::string receive() override;
private:
    std::mutex m;
    std::condition_variable cv;
    std::optional<std::string> value;
};

} // namespace raft

#endif // RAFT_SYNC_CHANNEL_H
