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
#ifndef RAFT_COMMAND_H
#define RAFT_COMMAND_H

#include <string>
#include "common/Util.hpp"
#include <memory>

namespace raft {

class StateMachine;
struct State;

struct Command {
    virtual ~Command() = default;
    virtual std::string serialize() const = 0;
    virtual std::unique_ptr<State> apply(raft::StateMachine& stateMachine) = 0;
    virtual bool operator==(const Command& other) const = 0;
    virtual bool operator!=(const Command& other) const = 0;
    virtual UUIDV7 getUUID() const {
        return uuid;
    };
    uint64_t term = 0;
    uint64_t index = 0;
protected:
    UUIDV7 uuid{};
};

} // namespace raft

#endif // RAFT_COMMAND_H
