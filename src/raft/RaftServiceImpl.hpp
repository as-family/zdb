#ifndef RAFT_SERVICE_IMPL_H
#define RAFT_SERVICE_IMPL_H

#include <memory>
#include <grpcpp/grpcpp.h>
#include "proto/raft.pb.h"
#include "proto/raft.grpc.pb.h"
#include "server/RPCServer.hpp"
#include "raft/Channel.hpp"
#include "raft/Raft.hpp"

namespace raft {

class RaftServiceImpl final : public proto::Raft::Service {
public:
    RaftServiceImpl(Raft* r);
    grpc::Status appendEntries(
        grpc::ServerContext* context,
        const proto::AppendEntriesArg* request,
        proto::AppendEntriesReply* response) override;
    grpc::Status requestVote(
        grpc::ServerContext* context,
        const proto::RequestVoteArg* request,
        proto::RequestVoteReply* response) override;
private:
    Raft* raft;
};

using RaftServer = zdb::RPCServer<RaftServiceImpl>;

} // namespace raft

#endif // RAFT_SERVICE_IMPL_H
