#include "RaftTestFramework/ProxyRaftService.hpp"
#include <grpcpp/grpcpp.h>
#include "common/ErrorConverter.hpp"

ProxyRaftService::ProxyRaftService(ProxyService<raft::proto::Raft>& p)
    : proxy{p} {}

grpc::Status ProxyRaftService::appendEntries(
    grpc::ServerContext* context,
    const raft::proto::AppendEntriesArg* request,
    raft::proto::AppendEntriesReply* response) {
    auto t = proxy.call("appendEntries", &raft::proto::Raft::Stub::appendEntries, *request, *response);
    if (t.has_value()) {
        return grpc::Status::OK;
    } else {
        return zdb::toGrpcStatus(t.error()[0]);
    }
}

grpc::Status ProxyRaftService::requestVote(
    grpc::ServerContext* context,
    const raft::proto::RequestVoteArg* request,
    raft::proto::RequestVoteReply* response) {
    auto t = proxy.call("requestVote", &raft::proto::Raft::Stub::requestVote, *request, *response);
    if (t.has_value()) {
        return grpc::Status::OK;
    } else {
        return zdb::toGrpcStatus(t.error()[0]);
    }
}
