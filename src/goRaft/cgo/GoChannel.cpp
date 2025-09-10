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
#include "GoRPCClient.hpp"
#include <queue>
#include "raft/RaftImpl.hpp"
#include <unordered_map>
#include <string>
#include "raft/Channel.hpp"
#include <memory>
#include "common/RetryPolicy.hpp"
#include <chrono>
#include <optional>
#include "proto/types.pb.h"

struct RaftHandle {
    int id;
    int servers;
    std::string selfId;
    std::unordered_map<std::string, std::queue<uint64_t>> commands;
    std::vector<std::string> peers;
    raft::Channel* serviceChannel;
    raft::Channel* followerChannel;
    zdb::RetryPolicy policy;
    uintptr_t callback;
    uintptr_t channelCallback;
    std::unique_ptr<raft::Channel> goChannel;
    std::unordered_map<std::string, int> peerIds;
    std::unordered_map<std::string, std::unique_ptr<GoRPCClient>> clients;
    std::unique_ptr<raft::RaftImpl<GoRPCClient>> raft;
};

extern "C" int channel_go_invoke_callback(uintptr_t handle, void *cmd, int cmd_size, int index);

GoChannel::GoChannel(uintptr_t h, RaftHandle* r)
    : handle{h}, raftHandle {r} {}

GoChannel::~GoChannel() {
}

void GoChannel::send(std::string) {
}

bool GoChannel::sendUntil(std::string command, std::chrono::system_clock::time_point t) {
    // std::cerr << "C++ command: " << command;
    // for (;!raftHandle->commands.at(command).empty(); raftHandle->commands.at(command).pop()) {
    //     std::cerr << raftHandle->commands.at(command).front() << " ";
    // }
    // std::cerr << "\n";
    zdb::proto::Command protoCommand;
    if (!protoCommand.ParseFromString(command)) {
        throw std::runtime_error("GoChannel::sendUntil: failed to parse command");
    }
    channel_go_invoke_callback(handle, (void*)protoCommand.op().data(), protoCommand.op().size(), protoCommand.index());
    // std::cerr << raftHandle->selfId << " C++: Sent command to Go, command=" << command << " index=" << protoCommand.index() << "\n";
    return true;
}

std::string GoChannel::receive() {
    // TODO: Implement Go channel receive
    return "";
}

std::optional<std::string> GoChannel::receiveUntil(std::chrono::system_clock::time_point t) {
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
