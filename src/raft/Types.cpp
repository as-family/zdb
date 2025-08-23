#include "raft/Types.hpp"

namespace raft {

AppendEntriesArg::AppendEntriesArg(const proto::AppendEntriesArg& arg)
    : leaderId(arg.leaderid()),
      term(arg.term()),
      prevLogIndex(arg.prevlogindex()),
      prevLogTerm(arg.prevlogterm()),
      leaderCommit(arg.leadercommit()),
      entries() {
    for (const auto& entry : arg.entries()) {
        entries.push_back(entry);
    }
}

RequestVoteArg::RequestVoteArg(const proto::RequestVoteArg& arg)
    : candidateId(arg.candidateid()),
      term(arg.term()),
      lastLogIndex(arg.lastlogindex()),
      lastLogTerm(arg.lastlogterm()) {}

} // namespace raft