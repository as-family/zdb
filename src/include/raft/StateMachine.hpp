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
#ifndef RAFT_STATE_MACHINE_H
#define RAFT_STATE_MACHINE_H

#include "raft/Log.hpp"
#include <unordered_map>
#include <string>
#include <memory>

namespace raft {

struct Command;

struct State {
    virtual ~State() = default;
};

class StateMachine {
public:
    virtual ~StateMachine() = default;

    virtual std::unique_ptr<State> applyCommand(raft::Command& command) = 0;
    virtual void consumeChannel() = 0;
    virtual std::unique_ptr<raft::State> handle(std::shared_ptr<raft::Command> c, std::chrono::system_clock::time_point t) = 0;
    virtual void snapshot() = 0;
    virtual void installSnapshot(uint64_t lastIncludedIndex, uint64_t lastIncludedTerm, const std::string& data) = 0;
};

} // namespace raft

#endif // RAFT_STATE_MACHINE_H
