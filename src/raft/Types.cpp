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
#include "raft/Types.hpp"

#include "common/Command.hpp"
#include "raft/Log.hpp"
#include <memory>

namespace raft {

AppendEntriesArg::AppendEntriesArg(const proto::AppendEntriesArg& arg)
    : leaderId(arg.leaderid()),
      term(arg.term()),
      prevLogIndex(arg.prevlogindex()),
      prevLogTerm(arg.prevlogterm()),
      leaderCommit(arg.leadercommit()) {
    for (const auto& entry : arg.entries()) {
        auto e = LogEntry {entry.index(), entry.term(), zdb::commandFactory(entry.command())};
        entries.append(e);
    }
}

AppendEntriesArg::AppendEntriesArg(std::string l, uint64_t t, uint64_t pi, uint64_t pt, uint64_t c, const Log& g)
    : leaderId{l},
      term {t},
      prevLogIndex{pi},
      prevLogTerm{pt},
      leaderCommit {c},
      entries{g.data()} {}

AppendEntriesArg::operator google::protobuf::Message&() {
    if (!protoArg) {
        protoArg = std::make_shared<proto::AppendEntriesArg>();
    }
    protoArg->set_leaderid(leaderId);
    protoArg->set_term(term);
    protoArg->set_prevlogindex(prevLogIndex);
    protoArg->set_prevlogterm(prevLogTerm);
    protoArg->set_leadercommit(leaderCommit);
    protoArg->clear_entries();
    for (const auto& e : entries.data()) {
        auto *entry = protoArg->add_entries();
        entry->set_index(e.index);
        entry->set_term(e.term);
        entry->set_command(e.command->serialize());
    }
    return *protoArg;
}

AppendEntriesArg::operator const google::protobuf::Message&() const {
    return const_cast<AppendEntriesArg*>(this)->operator google::protobuf::Message&();
}

AppendEntriesReply::AppendEntriesReply(bool cond, uint64_t t)
    : success{cond},
      term{t} {}

AppendEntriesReply::AppendEntriesReply(const google::protobuf::Message& m) {
    auto *reply = dynamic_cast<const proto::AppendEntriesReply*>(&m);
    if (!reply) {
        throw std::runtime_error("Invalid message type");
    }
    success = reply->success();
    term = reply->term();
    conflictTerm = reply->conflictterm();
    conflictIndex = reply->conflictindex();
}

AppendEntriesReply::operator google::protobuf::Message&() {
    if (!protoReply) {
        protoReply = std::make_shared<proto::AppendEntriesReply>();
    }
    protoReply->set_success(success);
    protoReply->set_term(term);
    protoReply->set_conflictterm(conflictTerm);
    protoReply->set_conflictindex(conflictIndex);
    return *protoReply;
}

AppendEntriesReply::operator const google::protobuf::Message&() const {
    return const_cast<AppendEntriesReply*>(this)->operator google::protobuf::Message&();
}

RequestVoteArg::RequestVoteArg(std::string c, uint64_t t, uint64_t li, uint64_t lt)
    : candidateId {c},
      term{t},
      lastLogIndex{li},
      lastLogTerm{lt} {}

RequestVoteArg::RequestVoteArg(const proto::RequestVoteArg& arg)
    : candidateId(arg.candidateid()),
      term(arg.term()),
      lastLogIndex(arg.lastlogindex()),
      lastLogTerm(arg.lastlogterm()) {}

RequestVoteArg::operator google::protobuf::Message&() {
    if (!protoArg) {
        protoArg = std::make_shared<proto::RequestVoteArg>();
    }
    protoArg->set_candidateid(candidateId);
    protoArg->set_term(term);
    protoArg->set_lastlogindex(lastLogIndex);
    protoArg->set_lastlogterm(lastLogTerm);
    return *protoArg;
}

RequestVoteArg::operator const google::protobuf::Message&() const {
    return const_cast<RequestVoteArg*>(this)->operator google::protobuf::Message&();
}


RequestVoteReply::RequestVoteReply(bool cond, uint64_t t)
    : voteGranted{cond},
      term{t} {}

RequestVoteReply::RequestVoteReply(const google::protobuf::Message& m) {
    auto *reply = dynamic_cast<const proto::RequestVoteReply*>(&m);
    if (!reply) {
        throw std::runtime_error("Invalid message type");
    }
    voteGranted = reply->votegranted();
    term = reply->term();
}

RequestVoteReply::operator google::protobuf::Message&() {
    if (!protoReply) {
        protoReply = std::make_shared<proto::RequestVoteReply>();
    }
    protoReply->set_votegranted(voteGranted);
    protoReply->set_term(term);
    return *protoReply;
}

RequestVoteReply::operator const google::protobuf::Message&() const {
    return const_cast<RequestVoteReply*>(this)->operator google::protobuf::Message&();
}

InstallSnapshotArg::InstallSnapshotArg(std::string l, uint64_t t, uint64_t si, uint64_t st, const std::string& sd)
    : leaderId{l},
      term{t},
      lastIncludedIndex{si},
      lastIncludedTerm{st},
      data{sd} {}

InstallSnapshotArg::InstallSnapshotArg(const proto::InstallSnapshotArg& arg)
    : leaderId(arg.leaderid()),
      term(arg.term()),
      lastIncludedIndex(arg.lastincludedindex()),
      lastIncludedTerm(arg.lastincludedterm()),
      data(arg.data()) {}

InstallSnapshotArg::operator google::protobuf::Message&() {
    if (!protoArg) {
        protoArg = std::make_shared<proto::InstallSnapshotArg>();
    }
    protoArg->set_leaderid(leaderId);
    protoArg->set_term(term);
    protoArg->set_lastincludedindex(lastIncludedIndex);
    protoArg->set_lastincludedterm(lastIncludedTerm);
    protoArg->set_data(data);
    return *protoArg;
}

InstallSnapshotArg::operator const google::protobuf::Message&() const {
    return const_cast<InstallSnapshotArg*>(this)->operator google::protobuf::Message&();
}

InstallSnapshotReply::InstallSnapshotReply(uint64_t t)
    : term{t}, success{true} {}

InstallSnapshotReply::InstallSnapshotReply(const google::protobuf::Message& m) {
    auto *reply = dynamic_cast<const proto::InstallSnapshotReply*>(&m);
    if (!reply) {
        throw std::runtime_error("Invalid message type");
    }
    term = reply->term();
    success = reply->success();
}

InstallSnapshotReply::operator google::protobuf::Message&() {
    if (!protoReply) {
        protoReply = std::make_shared<proto::InstallSnapshotReply>();
    }
    protoReply->set_term(term);
    protoReply->set_success(success);
    return *protoReply;
}

InstallSnapshotReply::operator const google::protobuf::Message&() const {
    return const_cast<InstallSnapshotReply*>(this)->operator google::protobuf::Message&();
}

} // namespace raft
