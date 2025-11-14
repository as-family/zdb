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

#ifndef RAFT_HANDLE_HPP
#define RAFT_HANDLE_HPP

#include <raft/RaftImpl.hpp>
#include <raft/Channel.hpp>
#include <string>
#include <unordered_map>
#include <memory>
#include "goRaft/cgo/GoRPCClient.hpp"
#include "storage/Persister.hpp"

struct RaftHandle {
    int id;
    int servers;
    std::string selfId;
    std::vector<std::string> peers;
    zdb::RetryPolicy policy;
    uintptr_t callback;
    uintptr_t channelCallback;
    std::shared_ptr<raft::Channel<std::shared_ptr<raft::Command>>> goChannel;
    std::unordered_map<std::string, int> peerIds;
    std::unordered_map<std::string, std::unique_ptr<GoRPCClient>> clients;
    std::shared_ptr<zdb::Persister> persister;
    std::shared_ptr<raft::RaftImpl<GoRPCClient>> raft;
};

#endif // RAFT_HANDLE_HPP
