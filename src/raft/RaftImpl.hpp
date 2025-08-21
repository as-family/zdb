#ifndef RAFT_IMPL_H
#define RAFT_IMPL_H

#include "raft/Raft.hpp"
#include "raft/Types.hpp"

namespace raft {

class RaftImpl : public Raft {
    AppendEntriesReply appendEntries(const AppendEntriesArg& arg) override;
    RequestVoteReply requestVote(const RequestVoteArg& arg) override;
    ~RaftImpl();
};

} // namespace raft

#endif // RAFT_IMPL_H
