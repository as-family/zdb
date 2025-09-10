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

#ifndef RAFT_TEST_RAFT_H
#define RAFT_TEST_RAFT_H

#include "raft/Raft.hpp"
#include "raft/Channel.hpp"
#include "raft/Types.hpp"
#include "raft/Log.hpp"

struct TestRaft : raft::Raft {
    TestRaft(raft::Channel& c) : channel {c}, mainLog{} {}
    raft::Start start(std::string cmd) override {
        channel.send(cmd);
        return {0, 0, true};
    }
    raft::AppendEntriesReply appendEntriesHandler(const raft::AppendEntriesArg& arg) override {
        std::ignore = arg;
        return {};
    }
    raft::RequestVoteReply requestVoteHandler(const raft::RequestVoteArg& arg) override {
        std::ignore = arg;
        return {};
    }
    void appendEntries(bool /*heartBeat*/) override {
    }
    void requestVote() override {
    }
    raft::Log& log() override {
        return mainLog;
    }
    void kill() override {
    }
    raft::Channel& channel;
private:
    raft::Log mainLog;
};

#endif // RAFT_TEST_RAFT_H
