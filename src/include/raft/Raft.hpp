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
#ifndef RAFT_H
#define RAFT_H

#include "raft/Log.hpp"
#include "raft/Types.hpp"
#include <optional>
#include <cstdint>
#include "raft/Command.hpp"
#include <chrono>
#include <string>
#include <unordered_map>
#include "raft/Command.hpp"
#include <memory>

namespace raft {

enum class Role {
    Follower,
    Candidate,
    Leader
};

struct PersistentState {
    uint64_t currentTerm = 0;
    std::optional<std::string> votedFor = std::nullopt;
    Log log;
    std::string snapshotData;
    PersistentState() = default;
    PersistentState(uint64_t term, std::optional<std::string> v, Log l, std::string sd)
        : currentTerm(term), votedFor(v), snapshotData(std::move(sd)) {
        log.clear();
        log.merge(l);
    }
    PersistentState(const PersistentState& p) {
        currentTerm = p.currentTerm;
        votedFor = p.votedFor;
        log.clear();
        log.merge(p.log);
        snapshotData = p.snapshotData;
    }
};

class Raft {
public:
    Raft(std::string id, std::chrono::milliseconds ht, std::chrono::milliseconds et, uint8_t cs)
        :selfId{id}, heartbeatInterval(ht), electionTimeout(et), clusterSize(cs) {}
    virtual ~Raft() = default;
    virtual AppendEntriesReply appendEntriesHandler(const AppendEntriesArg& arg) = 0;
    virtual RequestVoteReply requestVoteHandler(const RequestVoteArg& arg) = 0;
    virtual InstallSnapshotReply installSnapshotHandler(const InstallSnapshotArg& arg) = 0;
    virtual void appendEntries(std::string peerId) = 0;
    virtual void requestVote(std::string peerId) = 0;
    virtual bool start(std::shared_ptr<Command> c) = 0;
    virtual void snapshot(const uint64_t index, const std::string& snapshotData) = 0;
    virtual Log& log() = 0;
    virtual void kill() = 0;
    virtual Role getRole() const { return role; }
    virtual std::string getSelfId() const { return selfId; }
    virtual uint64_t getCurrentTerm() const { return currentTerm; }
    virtual void persist() = 0;
    virtual void readPersist(PersistentState) = 0;
protected:
    Role role = Role::Follower;
    std::string selfId;
    uint64_t currentTerm = 0;
    std::optional<std::string> votedFor = std::nullopt;
    uint64_t commitIndex = 0;
    uint64_t lastApplied = 0;
    std::unordered_map<std::string, uint64_t> nextIndex;
    std::unordered_map<std::string, uint64_t> matchIndex;
    std::chrono::milliseconds heartbeatInterval;
    std::chrono::milliseconds electionTimeout;
    uint8_t clusterSize;
    uint64_t lastIncludedIndex = 0;
    uint64_t lastIncludedTerm = 0;
};

} // namespace raft

#endif // RAFT_H
