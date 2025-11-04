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
#include "goRaft/cgo/raft_wrapper.hpp"
#include "raft/Raft.hpp"
#include "raft/RaftImpl.hpp"
#include "raft/Channel.hpp"
#include "proto/raft.pb.h"
#include "goRaft/cgo/GoRPCClient.hpp"
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdint>
#include <queue>
#include <unordered_map>
#include "common/Command.hpp"
#include "goRaft/cgo/GoChannel.hpp"
#include "goRaft/cgo/GoPersister.hpp"
#include "goRaft/cgo/RaftHandle.hpp"

extern "C" {

void kill_raft(RaftHandle* h) {
    if (!h) {
        return;
    }
    if (h->raft) {
        h->raft->kill();
        h->raft.reset();
    }
    if (h->goChannel) {
        h->goChannel->close();
        h->goChannel.reset();
    }
}

RaftHandle* create_raft(int id, int servers, uintptr_t cb, uintptr_t channelCb, uintptr_t pCb) {
    std::vector<std::string> peers;
    std::unordered_map<std::string, int> ids;
    std::string selfId = "peer_" + std::to_string(id);
    for (int i = 0; i < servers; ++i) {
        auto a = "peer_" + std::to_string(i);
        peers.push_back(a);
        ids[a] = i;
    }
    auto handle = new RaftHandle{
        id,
        servers,
        selfId,
        peers,
        zdb::RetryPolicy(
            std::chrono::milliseconds{2L},
            std::chrono::milliseconds{50L},
            std::chrono::milliseconds{60L},
            10,
            1,
            std::chrono::milliseconds{4L},
            std::chrono::milliseconds{4L}
        ),
        cb,
        channelCb,
        nullptr,
        ids,
        {},
        nullptr,
        nullptr
    };
    handle->goChannel = std::make_unique<GoChannel>(handle->channelCallback, handle);
    handle->persister = std::make_unique<GoPersister>(pCb);
    handle->raft = std::make_unique<raft::RaftImpl<GoRPCClient>>(
        peers,
        selfId,
        *handle->goChannel,
        handle->policy,
        [handle, cb](std::string address, zdb::RetryPolicy p, std::atomic<bool>& sc) -> GoRPCClient& {
            handle->clients[address] = std::make_unique<GoRPCClient>(handle->peerIds.at(address), address, p, cb, sc);
            return *(handle->clients[address]);
        },
        *handle->persister
    );
    return handle;
}

int handle_request_vote(RaftHandle* h, char* args, int args_size, char* reply) {
    if (!h || !h->raft) {
        return 0;
    }
    raft::proto::RequestVoteArg protoArgs{};
    auto s = std::string{args, static_cast<size_t>(args_size)};
    if (!protoArgs.ParseFromString(s)) {
        return 0;
    }
    auto r = h->raft->requestVoteHandler(protoArgs);
    raft::proto::RequestVoteReply protoReply{};
    protoReply.set_term(r.term);
    protoReply.set_votegranted(r.voteGranted);
    std::string reply_str;
    if (!protoReply.SerializeToString(&reply_str)) {
        return 0;
    }
    memcpy(reply, reply_str.data(), reply_str.size());
    return reply_str.size();
}

int handle_append_entries(RaftHandle* h, char* args, int args_size, char* reply) {
    if (!h || !h->raft) {
        return 0;
    }
    raft::proto::AppendEntriesArg protoArgs{};
    auto s = std::string {args, static_cast<size_t>(args_size)};
    if (!protoArgs.ParseFromString(s)) {
        return 0;
    }
    auto r = h->raft->appendEntriesHandler(protoArgs);
    raft::proto::AppendEntriesReply protoReply{};
    protoReply.set_success(r.success);
    protoReply.set_term(r.term);
    protoReply.set_conflictterm(r.conflictTerm);
    protoReply.set_conflictindex(r.conflictIndex);
    std::string reply_str;
    if (!protoReply.SerializeToString(&reply_str)) {
        return 0;
    }
    memcpy(reply, reply_str.data(), reply_str.size());
    return reply_str.size();
}

int handle_install_snapshot(RaftHandle* h, char* args, int args_size, char* reply) {
    if (!h || !h->raft) {
        return 0;
    }
    raft::proto::InstallSnapshotArg protoArgs{};
    auto s = std::string {args, static_cast<size_t>(args_size)};
    if (!protoArgs.ParseFromString(s)) {
        return 0;
    }
    auto r = h->raft->installSnapshotHandler(protoArgs);
    raft::proto::InstallSnapshotReply protoReply{};
    protoReply.set_term(r.term);
    std::string reply_str;
    if (!protoReply.SerializeToString(&reply_str)) {
        return 0;
    }
    memcpy(reply, reply_str.data(), reply_str.size());
    return reply_str.size();
}


int raft_get_state(RaftHandle* handle, int* term, int* is_leader) {
    if (!handle || !handle->raft) {
        return 0;
    }
    try {
        auto current_term = handle->raft->getCurrentTerm();
        auto role = handle->raft->getRole();

        *term = current_term;
        *is_leader = (role == raft::Role::Leader) ? 1 : 0;

        return 1;
    } catch (const std::exception& e) {
        return 0;
    }
}

int raft_start(RaftHandle* handle, void* command, int command_size, int* index, int* term, int* is_leader) {
    if (!handle || !handle->raft) {
        return 0;
    }
    std::string command_str{static_cast<const char*>(command), static_cast<size_t>(command_size)};
    auto uuid = generate_uuid_v7();
    auto c = std::make_shared<zdb::TestCommand>(uuid, command_str);
    *is_leader = handle->raft->start(c);
    *index = c->index;
    *term = c->term;
    return 1;
}

void raft_persist(RaftHandle* handle) {
    if (!handle || !handle->raft) {
        return;
    }
    handle->raft->persist();
}
void raft_read_persist(RaftHandle* handle, void* data, int data_size) {
    if (!handle || !handle->raft) {
        return;
    }
    raft::proto::PersistentState protoState;
    if (!protoState.ParseFromString(std::string{static_cast<const char*>(data), static_cast<size_t>(data_size)})) {
        throw std::runtime_error("failed to parse PersistentState");
    }
    raft::PersistentState p;
    p.currentTerm = protoState.currentterm();
    if (protoState.has_votedfor()) {
        p.votedFor = protoState.votedfor();
    } else {
        p.votedFor = std::nullopt;
    }
    for (const auto& e : protoState.log()) {
        p.log.append(raft::LogEntry{
            e.index(),
            e.term(),
            zdb::commandFactory(e.command())
        });
    }
    p.snapshotData = protoState.snapshot();
    p.lastIncludedIndex = protoState.lastincludedindex();
    p.lastIncludedTerm = protoState.lastincludedterm();
    handle->raft->readPersist(p);
}

void raft_snapshot(RaftHandle* handle, uint64_t index, char* snapshot_data, int snapshot_size) {
    if (!handle || !handle->raft) {
        return;
    }
    std::string snapshot_str{snapshot_data, static_cast<size_t>(snapshot_size)};
    handle->raft->snapshot(index, snapshot_str);
}

}
