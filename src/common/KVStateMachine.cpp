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
#include <mutex>

namespace zdb {

KVStateMachine::KVStateMachine(StorageEngine& s, raft::Channel<std::shared_ptr<raft::Command>>& raftCh, raft::Raft& r)
    : storageEngine(s),
      raftChannel(raftCh),
      raft {r},
      consumerThread {std::thread(&KVStateMachine::consumeChannel, this)} {}

std::unique_ptr<raft::State> KVStateMachine::applyCommand(raft::Command& command) {
    return command.apply(*this);
}

void KVStateMachine::consumeChannel() {
    while (!raftChannel.isClosed()) {
        std::lock_guard lock{m};
        auto c = raftChannel.receiveUntil(std::chrono::system_clock::now() + std::chrono::milliseconds{1L});
        if (!c.has_value()) {
            break;
        }
        c.value()->apply(*this);
    }
}

void KVStateMachine::snapshot() {
    // Create a snapshot of the current state
}

void KVStateMachine::restore(const std::string& snapshot) {
    // Restore the state from a snapshot
    std::ignore = snapshot;
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

std::unique_ptr<raft::State> KVStateMachine::handle(std::shared_ptr<raft::Command> c, std::chrono::system_clock::time_point t) {
    std::lock_guard lock{m};
    if (!raft.start(c)) {
        return std::make_unique<State>(State{Error{ErrorCode::NotLeader, "not the leader"}});
    }
    while (true) {
        auto r = raftChannel.receiveUntil(t);
        if (!r.has_value()) {
            return std::make_unique<State>(State{Error{ErrorCode::Timeout, "request timed out"}});
        }
        auto s = applyCommand(*r.value());
        if (r.value()->getUUID() == c->getUUID()) {
            return s;
        }
    }
}

KVStateMachine::~KVStateMachine() {
    raftChannel.close();
    if (consumerThread.joinable()) {
        consumerThread.join();
    }
}

} // namespace zdb
