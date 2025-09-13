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

#include "goRaft/cgo/GoChannel.hpp"
#include "raft/RaftImpl.hpp"
#include <string>
#include <chrono>
#include <optional>
#include "proto/types.pb.h"
#include "RaftHandle.hpp"

extern "C" int channel_go_invoke_callback(uintptr_t handle, void *cmd, int cmd_size, int index);

GoChannel::GoChannel(uintptr_t h, RaftHandle* r)
    : handle{h}, raftHandle {r} {}

GoChannel::~GoChannel() {
}

void GoChannel::send(std::shared_ptr<raft::Command>) {
}

bool GoChannel::sendUntil(std::shared_ptr<raft::Command> command, std::chrono::system_clock::time_point t) {
    auto c = command->serialize();
    channel_go_invoke_callback(handle, (void*)c.data(), c.size(), command->index);
    return true;
}

std::optional<std::shared_ptr<raft::Command>> GoChannel::receive() {
    // TODO: Implement Go channel receive
    return std::nullopt;
}

std::optional<std::shared_ptr<raft::Command>> GoChannel::receiveUntil(std::chrono::system_clock::time_point t) {
    // TODO: Implement Go channel receive with timeout
    std::ignore = t;
    return std::nullopt;
}

void GoChannel::close() {
    // TODO: Implement Go channel close
}

bool GoChannel::isClosed() {
    // TODO: Implement Go channel closed check
    return false;
}
