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

#ifndef RSM_HANDLE_H
#define RSM_HANDLE_H

#include <raft/StateMachine.hpp>
#include <raft/Channel.hpp>
#include <raft/Command.hpp>
#include <storage/Persister.hpp>
#include <storage/StorageEngine.hpp>
#include <goRaft/cgo/RaftHandle.hpp>
#include <goRaft/cgo/GoStateMachine.hpp>
#include <raft/Rsm.hpp>

struct RsmHandle {
    int id;
    int servers;
    int maxraftstate;
    uintptr_t goChannelCb;
    uintptr_t persisterCb;
    std::shared_ptr<zdb::Persister> persister;
    std::shared_ptr<raft::Channel<std::shared_ptr<raft::Command>>> goChannel;
    std::shared_ptr<GoStateMachine> machine;
    std::unique_ptr<raft::Rsm> rsm;
    std::shared_ptr<raft::Raft> raft;
    RaftHandle* raftHandle;
};

#endif // RSM_HANDLE_H
