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

#include <goRaft/cgo/GoStateMachine.hpp>
#include <proto/kvStore.pb.h>
#include <spdlog/spdlog.h>
#include <common/Types.hpp>
#include <common/Command.hpp>

GoStateMachine::GoStateMachine(uintptr_t h)
    : handle {h} {}

std::unique_ptr<raft::State> GoStateMachine::applyCommand(raft::Command& command) {
    std::string c {command.serialize()};
    int maxSize = 4096;
    std::string stateBuffer(maxSize, 0);
    int size = state_machine_go_apply_command(handle, c.data(), c.size(), stateBuffer.data());
    if (size < 0) {
        spdlog::error(" GoStateMachine::applyCommand: bad command application");
        return std::make_unique<zdb::State>(zdb::Error{zdb::ErrorCode::Unknown});
    }
    if (size > maxSize) {
        spdlog::error(" GoStateMachine::applyCommand: state size {} exceeds buffer size {}", size, maxSize);
        return std::make_unique<zdb::State>(zdb::Error{zdb::ErrorCode::Internal});
    }
    stateBuffer.resize(size);
    return zdb::State::fromString(stateBuffer);
}

std::shared_ptr<raft::Command> GoStateMachine::snapshot() {
    auto u = generate_uuid_v7();
    return std::make_shared<zdb::InstallSnapshotCommand>(u, 0, 0, "");
}

void GoStateMachine::installSnapshot(std::shared_ptr<raft::Command>) {

}
