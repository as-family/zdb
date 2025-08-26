#include "RaftTestFramework/ProxyRaftService.hpp"
#include <grpcpp/grpcpp.h>

ProxyRaftService::ProxyRaftService(ProxyService<raft::proto::Raft>& p)
    : proxy{p} {}

grpc::Status ProxyRaftService::appendEntries(
    grpc::ServerContext* context,
    const raft::proto::AppendEntriesArg* request,
    raft::proto::AppendEntriesReply* response) {
    return proxy.call(&raft::proto::Raft::Stub::appendEntries, request, response);
}

grpc::Status ProxyRaftService::requestVote(
    grpc::ServerContext* context,
    const raft::proto::RequestVoteArg* request,
    raft::proto::RequestVoteReply* response) {
    return proxy.call(&raft::proto::Raft::Stub::requestVote, request, response);
}
