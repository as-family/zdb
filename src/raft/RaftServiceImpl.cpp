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
#include "raft/RaftServiceImpl.hpp"
#include "raft/Types.hpp"

namespace raft {

RaftServiceImpl::RaftServiceImpl(Raft& r) : raft(r) {
}

grpc::Status RaftServiceImpl::requestVote(
    grpc::ServerContext* /*context*/,
    const proto::RequestVoteArg* request,
    proto::RequestVoteReply* reply) {
    auto r = raft.requestVoteHandler(*request);
    reply->set_term(r.term);
    reply->set_votegranted(r.voteGranted);
    return grpc::Status::OK;
}

grpc::Status RaftServiceImpl::appendEntries(
    grpc::ServerContext* /*context*/,
    const proto::AppendEntriesArg* request,
    proto::AppendEntriesReply* reply) {
    AppendEntriesArg arg {*request};
    auto r = raft.appendEntriesHandler(arg);
    reply->set_success(r.success);
    reply->set_term(r.term);
    reply->set_conflictindex(r.conflictIndex);
    reply->set_conflictterm(r.conflictTerm);
    return grpc::Status::OK;
}

grpc::Status RaftServiceImpl::installSnapshot(
    grpc::ServerContext* /*context*/,
    const proto::InstallSnapshotArg* request,
    proto::InstallSnapshotReply* reply) {
    InstallSnapshotArg arg{*request};
    auto r = raft.installSnapshotHandler(arg);
    reply->set_term(r.term);
    return grpc::Status::OK;
}

} // namespace raft
