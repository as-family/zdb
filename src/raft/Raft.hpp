#ifndef RAFT_H
#define RAFT_H

#include "raft/Log.hpp"
#include "raft/Types.hpp"
#include <optional>
#include <vector>
#include <cstdint>

namespace raft {

class Raft {
public:
    uint64_t currentTerm = 0;
    std::optional<uint8_t> votedFor;
    Log log;
    uint64_t commitIndex = 0;
    uint64_t lastApplied = 0;
    std::vector<uint64_t> nextIndex;
    std::vector<uint64_t> matchIndex;
    virtual ~Raft() = default;
    virtual AppendEntriesReply appendEntries(const AppendEntriesArg& arg) = 0;
    virtual RequestVoteReply requestVote(const RequestVoteArg& arg) = 0;
};

} // namespace raft

#endif // RAFT_H
