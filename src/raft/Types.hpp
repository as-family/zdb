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
#ifndef RAFT_TYPES_H
#define RAFT_TYPES_H

#include <cstdint>
#include <vector>
#include "raft/Log.hpp"
#include <proto/raft.pb.h>
#include <string>

namespace raft {

struct AppendEntriesArg {
    std::string leaderId;
    uint64_t term;
    uint64_t prevLogIndex;
    uint64_t prevLogTerm;
    uint64_t leaderCommit;
    Log entries;
    AppendEntriesArg(std::string, uint64_t, uint64_t, uint64_t, uint64_t, const Log&);
    AppendEntriesArg(const proto::AppendEntriesArg& arg);
};

struct AppendEntriesReply {
    bool success;
    uint64_t term;
};

struct RequestVoteArg {
    std::string candidateId;
    uint64_t term;
    uint64_t lastLogIndex;
    uint64_t lastLogTerm;
    RequestVoteArg(const proto::RequestVoteArg& arg);
};

struct RequestVoteReply {
    bool voteGranted;
    uint64_t term;
};

} // namespace raft

#endif // RAFT_TYPES_H
