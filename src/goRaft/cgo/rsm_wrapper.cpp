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

#include "goRaft/cgo/rsm_wrapper.hpp"
#include "goRaft/cgo/RsmHandle.hpp"
#include <goRaft/cgo/GoChannel.hpp>
#include <goRaft/cgo/GoPersister.hpp>
#include <common/KVStateMachine.hpp>
#include <storage/InMemoryKVStore.hpp>
#include <goRaft/cgo/RaftHandle.hpp>
#include <raft/Rsm.hpp>
#include <spdlog/spdlog.h>
#include <proto/types.pb.h>

RsmHandle* create_rsm(int id, int servers, uintptr_t rpc, uintptr_t channel, uintptr_t recChannel, uintptr_t persister, int maxraftstate, uintptr_t sm) {
    auto handle = new RsmHandle {
        id,
        servers,
        maxraftstate,
        channel,
        persister
    };
    handle->raftHandle = create_raft(id, servers, rpc, channel, recChannel, persister);
    if (handle->raftHandle == nullptr) {
        delete handle;
        return nullptr;
    }
    handle->goChannel = handle->raftHandle->goChannel;
    handle->persister = handle->raftHandle->persister;
    handle->machine = std::make_shared<GoStateMachine>(sm);
    handle->raft = handle->raftHandle->raft;
    handle->rsm = std::make_unique<raft::Rsm>(handle->machine, handle->goChannel, handle->raftHandle->raft);
    return handle;
}

int rsm_submit(RsmHandle* handle, void* command, int command_size, void* state) {
    if (!handle || !handle->rsm || !handle->raftHandle || !handle->raftHandle->raft) {
        spdlog::error("rsm_submit: bad handle");
        return -1;
    }
    std::string commandBuffer((char*) command, command_size);
    auto c = zdb::commandFactory(commandBuffer);
    auto s = handle->rsm->handle(c, std::chrono::system_clock::now() + std::chrono::seconds(10L));
    auto out = s->toProto().SerializeAsString();
    memcpy(state, out.data(), out.size());
    return out.size();
}

RaftHandle* rsm_raft_handle(RsmHandle* handle) {
    return handle->raftHandle;
}
