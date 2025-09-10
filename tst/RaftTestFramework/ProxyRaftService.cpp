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
