#include "raft/RaftServiceImpl.hpp"
#include "raft/Types.hpp"

namespace raft {

RaftServiceImpl::RaftServiceImpl(Raft* r) : raft(r) {}

grpc::Status RaftServiceImpl::requestVote(
    grpc::ServerContext* context,
    const proto::RequestVoteArg* request,
    proto::RequestVoteReply* reply) {
    auto r = raft->requestVoteHandler(*request);
    reply->set_term(r.term);
    reply->set_votegranted(r.voteGranted);
    return grpc::Status::OK;
}

grpc::Status RaftServiceImpl::appendEntries(
    grpc::ServerContext* context,
    const proto::AppendEntriesArg* request,
    proto::AppendEntriesReply* reply) {
    auto r = raft->appendEntriesHandler(*request);
    reply->set_success(r.success);
    reply->set_term(r.term);
    return grpc::Status::OK;
}

} // namespace raft
