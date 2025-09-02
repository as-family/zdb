#ifndef RAFT_CHANNEL_H
#define RAFT_CHANNEL_H

#include <optional>
#include <string>
#include <chrono>

namespace raft {

class Channel {
public:
    virtual ~Channel() = default;
    virtual void send(std::string) = 0;
    virtual std::string receive() = 0;
    virtual std::optional<std::string> receiveUntil(std::chrono::system_clock::time_point t) = 0;
    virtual void close() = 0;
    virtual bool isClosed() = 0;
};

} // namespace raft

#endif // RAFT_CHANNEL_H
