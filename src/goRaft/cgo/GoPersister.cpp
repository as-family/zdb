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

#include "goRaft/cgo/GoPersister.hpp"
#include "raft/Raft.hpp"
#include "proto/raft.pb.h"
#include "goRaft/cgo/raft_wrapper.hpp"

GoPersister::GoPersister(uintptr_t h) : handle(h) {
}

std::string GoPersister::loadBuffer() {
    constexpr int bufferSize = 65536;
    std::string buffer(bufferSize, 0);
    int len = persister_go_read_callback(handle, buffer.data(), bufferSize);
    if (len <= 0) {
        return {};
    }
    return buffer;
}

raft::PersistentState GoPersister::load() {
    return {};
}

void GoPersister::save(raft::PersistentState s) {
    raft::proto::PersistentState p;
    p.set_currentterm(s.currentTerm);
    if (s.votedFor.has_value()) {
        p.set_votedfor(s.votedFor.value());
    }
    for (auto &e : s.log.data()) {
        auto entry = p.add_log();
        entry->set_term(e.term);
        entry->set_index(e.index);
        entry->set_command(e.command->serialize());
    }
    p.set_snapshot(s.snapshotData);
    p.set_lastincludedindex(s.lastIncludedIndex);
    p.set_lastincludedterm(s.lastIncludedTerm);
    auto str = p.SerializeAsString();
    persister_go_invoke_callback(handle, str.data(), str.size());
}

GoPersister::~GoPersister() = default;
