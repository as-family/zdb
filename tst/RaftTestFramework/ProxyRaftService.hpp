// SPDX-License-Identifier: AGPL-3.0-or-later
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
