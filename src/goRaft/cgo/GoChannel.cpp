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
#include "goRaft/cgo/RaftHandle.hpp"
#include <spdlog/spdlog.h>

extern "C" int channel_go_invoke_callback(uintptr_t handle, void *cmd, int cmd_size, int index);
extern "C" int receive_channel_go_callback(uintptr_t handle, void *command);
extern "C" void channel_close_callback(uintptr_t handle);
extern "C" int channel_is_closed_callback(uintptr_t handle);

GoChannel::GoChannel(uintptr_t h, uintptr_t h2, uintptr_t h3, RaftHandle* r)
    : handle{h}, recHandle{h2}, closeHandle{h3}, raftHandle {r} {}

GoChannel::~GoChannel() {
}

void GoChannel::send(std::shared_ptr<raft::Command>) {
}

bool GoChannel::sendUntil(std::shared_ptr<raft::Command> command, std::chrono::system_clock::time_point t) {
    auto c = command->serialize();
    return channel_go_invoke_callback(handle, (void*)c.data(), c.size(), command->index) == 0;
}

std::optional<std::shared_ptr<raft::Command>> GoChannel::receive() {
    // TODO: Implement Go channel receive
    return std::nullopt;
}

std::optional<std::shared_ptr<raft::Command>> GoChannel::receiveUntil(std::chrono::system_clock::time_point t) {
    std::ignore = t;
    std::string buffer(1024, 0);
    int size = receive_channel_go_callback(recHandle, buffer.data());
    if (size < 0) {
        // spdlog::error("GoChannel::receiveUntil bad command");
        return std::nullopt;
    }
    buffer.resize(size);
    return zdb::commandFactory(buffer);
}

void GoChannel::close() {
    spdlog::info("GoChannel::close handle {} {}", handle, fmt::ptr(this));
    channel_close_callback(closeHandle);
}

bool GoChannel::isClosed() {
    return channel_is_closed_callback(closeHandle);
}
