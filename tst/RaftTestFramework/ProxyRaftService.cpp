#include "RaftTestFramework/ProxyRaftService.hpp"
#include <grpcpp/grpcpp.h>
#include "common/ErrorConverter.hpp"

ProxyRaftService::ProxyRaftService(ProxyService<raft::proto::Raft>& p)
    : proxy{p} {}

grpc::Status ProxyRaftService::appendEntries(
    grpc::ServerContext* /*context*/,
    const raft::proto::AppendEntriesArg* request,
    raft::proto::AppendEntriesReply* response) {
    auto t = proxy.call("appendEntries", *request, *response);
    if (t.has_value()) {
        return grpc::Status::OK;
    } else {
        const auto& errs = t.error();  
        return errs.empty()
            ? grpc::Status(grpc::StatusCode::UNKNOWN, "appendEntries: empty error stack")
            : zdb::toGrpcStatus(errs.front());
    }
}

grpc::Status ProxyRaftService::requestVote(
    grpc::ServerContext* /*context*/,
    const raft::proto::RequestVoteArg* request,
    raft::proto::RequestVoteReply* response) {
    auto t = proxy.call("requestVote", *request, *response);
    if (t.has_value()) {
        return grpc::Status::OK;
    } else {
        const auto& errs = t.error();
        return errs.empty()
            ? grpc::Status(grpc::StatusCode::UNKNOWN, "requestVote: empty error stack")
            : zdb::toGrpcStatus(errs.front());
    }
}
