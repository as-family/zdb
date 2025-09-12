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
#ifndef RAFT_TYPES_H
#define RAFT_TYPES_H

#include <cstdint>
#include <vector>
#include "raft/Log.hpp"
#include <proto/raft.pb.h>
#include <string>

namespace raft {

struct Arg {
    virtual operator google::protobuf::Message&() = 0;
    virtual operator const google::protobuf::Message&() const = 0;
    virtual ~Arg() = default;
};

struct Reply {
    virtual operator google::protobuf::Message&() = 0;
    virtual operator const google::protobuf::Message&() const = 0;
    virtual ~Reply() = default;
};

struct AppendEntriesArg : Arg {
    std::string leaderId;
    uint64_t term;
    uint64_t prevLogIndex;
    uint64_t prevLogTerm;
    uint64_t leaderCommit;
    Log entries;
    AppendEntriesArg(std::string, uint64_t, uint64_t, uint64_t, uint64_t, const Log&);
    AppendEntriesArg(const proto::AppendEntriesArg& arg);
    operator google::protobuf::Message&() override;
    operator const google::protobuf::Message&() const override;
    ~AppendEntriesArg() override = default;
private:
    std::shared_ptr<proto::AppendEntriesArg> protoArg;
};

struct AppendEntriesReply : Reply {
    bool success;
    uint64_t term;
    AppendEntriesReply(bool cond, uint64_t uint64);
    AppendEntriesReply(const google::protobuf::Message& m);
    operator google::protobuf::Message&() override;
    operator const google::protobuf::Message&() const override;
    ~AppendEntriesReply() override = default;
private:
    std::shared_ptr<proto::AppendEntriesReply> protoReply;
};

struct RequestVoteArg : Arg {
    std::string candidateId;
    uint64_t term;
    uint64_t lastLogIndex;
    uint64_t lastLogTerm;
    RequestVoteArg(const proto::RequestVoteArg& arg);
    operator google::protobuf::Message&() override;
    operator const google::protobuf::Message&() const override;
    ~RequestVoteArg() override = default;
private:
    std::shared_ptr<proto::RequestVoteArg> protoArg;
};

struct RequestVoteReply : Reply {
    bool voteGranted;
    uint64_t term;
    RequestVoteReply(bool cond, uint64_t uint64);
    RequestVoteReply(const google::protobuf::Message& m);
    operator google::protobuf::Message&() override;
    operator const google::protobuf::Message&() const override;
    ~RequestVoteReply() override = default;
private:
    std::shared_ptr<proto::RequestVoteReply> protoReply;
};

} // namespace raft

#endif // RAFT_TYPES_H
