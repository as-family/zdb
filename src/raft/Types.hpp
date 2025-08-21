#ifndef RAFT_TYPES_H
#define RAFT_TYPES_H

#include <cstdint>
#include <vector>
#include "raft/Log.hpp"

namespace raft {

struct AppendEntriesArg {
    uint8_t leaderId;
    uint64_t term;
    uint64_t prevLogIndex;
    uint64_t prevLogTerm;
    uint64_t leaderCommit;
    std::vector<LogEntry> entries;
};

struct AppendEntriesReply {
    bool success;
    uint64_t term;
};

struct RequestVoteArg {
    uint8_t candidateId;
    uint64_t term;
    uint64_t lastLogIndex;
    uint64_t lastLogTerm;

};

struct RequestVoteReply {
    bool voteGranted;
    uint64_t term;
};

} // namespace raft

#endif // RAFT_TYPES_H
