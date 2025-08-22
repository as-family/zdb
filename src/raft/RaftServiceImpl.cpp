#include "raft/RaftServiceImpl.hpp"

namespace raft {

RaftServiceImpl::RaftServiceImpl(Raft* r) : raft(r) {}

grpc::Status RaftServiceImpl::requestVote(
    grpc::ServerContext* context,
    const proto::RequestVoteArg* request,
    proto::RequestVoteReply* reply) {
    reply->set_term(raft->currentTerm);
    if (request->term() < raft->currentTerm ||
        raft->votedFor.has_value() && raft->votedFor.value() != request->candidateid()) {
        reply->set_votegranted(false);
    } else if (request->lastlogindex() >= raft->log.lastIndex() &&
               request->lastlogterm() >= raft->log.lastTerm()) {
        raft->votedFor = request->candidateid();
        reply->set_votegranted(true);
    }
    return grpc::Status::OK;
}

grpc::Status RaftServiceImpl::appendEntries(
    grpc::ServerContext* context,
    const proto::AppendEntriesArg* request,
    proto::AppendEntriesReply* reply) {
    // Implement your logic here
    return grpc::Status::OK;
}

} // namespace raft
