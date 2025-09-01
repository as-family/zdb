#ifndef RAFT_SYNC_CHANNEL_H
#define RAFT_SYNC_CHANNEL_H

#include "raft/Command.hpp"
#include <mutex>
#include <optional>
#include <condition_variable>
#include "raft/Channel.hpp"
#include <cstddef>
#include <optional>

namespace raft {

class SyncChannel : public Channel {
public:
    void send(std::string) override;
    std::string receive() override;
    std::optional<std::string> receiveUntil(std::chrono::system_clock::time_point t) override;
    virtual void close() override;
    virtual bool isClosed() override;
    ~SyncChannel();
private:
    std::mutex m;
    std::condition_variable cv;
    std::optional<std::string> value;
    bool closed = false;
};

} // namespace raft

#endif // RAFT_SYNC_CHANNEL_H
