#ifndef PROXY_RAFT_SERVICE_H
#define PROXY_RAFT_SERVICE_H

#include "KVTestFramework/ProxyService.hpp"
#include "proto/raft.grpc.pb.h"

class ProxyRaftService : public raft::proto::Raft::Service {
public:
    ProxyRaftService(ProxyService<raft::proto::Raft>& p);
    grpc::Status appendEntries(
        grpc::ServerContext* context,
        const raft::proto::AppendEntriesArg* request,
        raft::proto::AppendEntriesReply* response) override;
    grpc::Status requestVote(
        grpc::ServerContext* context,
        const raft::proto::RequestVoteArg* request,
        raft::proto::RequestVoteReply* response) override;
private:
    ProxyService<raft::proto::Raft>& proxy;
};

#endif // PROXY_RAFT_SERVICE_H
