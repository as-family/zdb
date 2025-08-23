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

namespace raft {

enum class Role {
    Follower,
    Candidate,
    Leader
};

class Raft {
public:
    Role role = Role::Follower;
    std::string selfId;
    uint64_t currentTerm = 0;
    std::optional<std::string> votedFor = std::nullopt;
    Log log {};
    uint64_t commitIndex = 0;
    uint64_t lastApplied = 0;
    std::vector<uint64_t> nextIndex{};
    std::vector<uint64_t> matchIndex{};
    std::chrono::milliseconds electionTimeout;
    std::chrono::milliseconds heartbeatInterval;
    std::vector<std::string> peerAddresses;
    virtual ~Raft() = default;
    virtual AppendEntriesReply appendEntriesHandler(const AppendEntriesArg& arg) = 0;
    virtual RequestVoteReply requestVoteHandler(const RequestVoteArg& arg) = 0;
    virtual void appendEntries() = 0;
    virtual void requestVote() = 0;
    virtual void start(Command* command) = 0;
    virtual void kill() = 0;
};

} // namespace raft

#endif // RAFT_H
