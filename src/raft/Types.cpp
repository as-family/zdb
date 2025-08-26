#include "raft/Types.hpp"
#include "raft/Log.hpp"

namespace raft {

AppendEntriesArg::AppendEntriesArg(const proto::AppendEntriesArg& arg, Log& log)
    : leaderId(arg.leaderid()),
      term(arg.term()),
      prevLogIndex(arg.prevlogindex()),
      prevLogTerm(arg.prevlogterm()),
      leaderCommit(arg.leadercommit()), 
      entries(log) {
    for (const auto& entry : arg.entries()) {
        entries.append(entry);
    }
}

AppendEntriesArg::AppendEntriesArg(std::string l, uint64_t t, uint64_t pi, uint64_t pt, uint64_t c, Log& g)
    : leaderId{l},
      term {t},
      prevLogIndex{pi},
      prevLogTerm{pt},
      leaderCommit {c},
      entries{g} {}

RequestVoteArg::RequestVoteArg(const proto::RequestVoteArg& arg)
    : candidateId(arg.candidateid()),
      term(arg.term()),
      lastLogIndex(arg.lastlogindex()),
      lastLogTerm(arg.lastlogterm()) {}

} // namespace raft
