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

#include "common/KVStateMachine.hpp"
#include "raft/StateMachine.hpp"
#include "raft/Command.hpp"
#include "common/Command.hpp"
#include "common/Util.hpp"
#include <mutex>

namespace zdb {

KVStateMachine::KVStateMachine(StorageEngine& s)
    : storageEngine(s) {}

std::unique_ptr<raft::State> KVStateMachine::applyCommand(raft::Command& command) {
    return command.apply(*this);
}

std::shared_ptr<raft::Command> KVStateMachine::snapshot() {
    // Create a snapshot of the current state
    return nullptr;
}

void KVStateMachine::installSnapshot(std::shared_ptr<raft::Command>) {
    // Restore the state from a snapshot
}

State KVStateMachine::get(Key key) {
    auto result = storageEngine.get(key);
    return State{key, result};
}

State KVStateMachine::set(Key key, Value value) {
    auto result = storageEngine.set(key, value);
    return State{key, result};
}

State KVStateMachine::erase(Key key) {
    auto result = storageEngine.erase(key);
    return State{key, result};
}

State KVStateMachine::size() {
    auto result = storageEngine.size();
    return State{result};
}

} // namespace zdb
