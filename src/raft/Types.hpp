#ifndef RAFT_TYPES_H
#define RAFT_TYPES_H

#include <cstdint>
#include <vector>
#include "raft/Log.hpp"
#include <proto/raft.pb.h>
#include <string>
#include "raft/Log.hpp" 

namespace raft {

struct AppendEntriesArg {
    std::string leaderId;
    uint64_t term;
    uint64_t prevLogIndex;
    uint64_t prevLogTerm;
    uint64_t leaderCommit;
    Log& entries;
    AppendEntriesArg(const proto::AppendEntriesArg& arg, Log& log);
};

struct AppendEntriesReply {
    bool success;
    uint64_t term;
};

struct RequestVoteArg {
    std::string candidateId;
    uint64_t term;
    uint64_t lastLogIndex;
    uint64_t lastLogTerm;
    RequestVoteArg(const proto::RequestVoteArg& arg);
};

struct RequestVoteReply {
    bool voteGranted;
    uint64_t term;
};

} // namespace raft

#endif // RAFT_TYPES_H
