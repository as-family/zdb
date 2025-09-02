#ifndef RAFT_H
#define RAFT_H

#include "raft/Log.hpp"
#include "raft/Types.hpp"
#include <optional>
#include <vector>
#include <cstdint>
#include "raft/Command.hpp"
#include <chrono>
#include <string>
#include <unordered_map>

namespace raft {

enum class Role {
    Follower,
    Candidate,
    Leader
};

class Raft {
public:
    virtual ~Raft() = default;
    virtual AppendEntriesReply appendEntriesHandler(const AppendEntriesArg& arg) = 0;
    virtual RequestVoteReply requestVoteHandler(const RequestVoteArg& arg) = 0;
    virtual void appendEntries(bool heartBeat) = 0;
    virtual void requestVote() = 0;
    virtual bool start(std::string command) = 0;
    virtual Log& log() = 0;
    virtual void kill() = 0;
    virtual Role getRole() const { return role; }
    virtual std::string getSelfId() const { return selfId; }
    virtual uint64_t getCurrentTerm() const { return currentTerm; }
protected:
    Role role = Role::Follower;
    std::string selfId;
    uint64_t currentTerm = 0;
    std::optional<std::string> votedFor = std::nullopt;
    uint64_t commitIndex = 0;
    uint64_t lastApplied = 0;
    std::unordered_map<std::string, uint64_t> nextIndex;
    std::unordered_map<std::string, uint64_t> matchIndex;
    std::chrono::milliseconds electionTimeout;
    std::chrono::milliseconds heartbeatInterval;
    uint8_t clusterSize;
};

} // namespace raft

#endif // RAFT_H
