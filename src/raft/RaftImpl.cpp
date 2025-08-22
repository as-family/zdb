#include "raft/RaftImpl.hpp"
#include "raft/Types.hpp"
#include <tuple>
#include <algorithm>
#include "raft/RaftServiceImpl.hpp"

namespace raft {

RaftImpl::RaftImpl(std::vector<std::string> p, std::string s, Channel& c)
    : serviceChannel(c),
      peerAddresses(std::move(p)),
      selfId(s),
      policy{std::chrono::milliseconds(10), std::chrono::milliseconds(100), std::chrono::milliseconds(1000), 3, 1},
      raftService {this},
      server {selfId, raftService} {
    peerAddresses.erase(std::find(peerAddresses.begin(), peerAddresses.end(), selfId));
    for (const auto& peer : peerAddresses) {
        peers.emplace(std::piecewise_construct,
                       std::forward_as_tuple(peer),
                       std::forward_as_tuple(peer, policy));
    }
}

AppendEntriesReply RaftImpl::appendEntries(const AppendEntriesArg& arg) {
    // Implementation of appendEntries logic
    AppendEntriesReply reply;
    // Logic to handle the append entries request
    return reply;
}

RequestVoteReply RaftImpl::requestVote(const RequestVoteArg& arg) {
    // Implementation of requestVote logic
    RequestVoteReply reply;
    // Logic to handle the request vote
    return reply;
}

RaftImpl::~RaftImpl() {
}

} // namespace raft
