/*
 * ZDB a distributed, fault-tolerant database.
 * Copyright (C) 2025 Ahmed Refaat Gadalla Mohamed
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
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
    RaftServiceImpl(Raft& r);
    RaftServiceImpl(const RaftServiceImpl&) = delete;
    RaftServiceImpl& operator=(const RaftServiceImpl&) = delete;
    RaftServiceImpl(RaftServiceImpl&&) = delete;
    RaftServiceImpl& operator=(RaftServiceImpl&&) = delete;
    ~RaftServiceImpl() override = default;
    grpc::Status appendEntries(
        grpc::ServerContext* context,
        const proto::AppendEntriesArg* request,
        proto::AppendEntriesReply* response) override;
    grpc::Status requestVote(
        grpc::ServerContext* context,
        const proto::RequestVoteArg* request,
        proto::RequestVoteReply* response) override;
private:
    Raft& raft;
};

} // namespace raft

#endif // RAFT_SERVICE_IMPL_H
