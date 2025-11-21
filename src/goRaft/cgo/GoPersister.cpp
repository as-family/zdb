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
#include "common/Command.hpp"
#include <spdlog/spdlog.h>

GoPersister::GoPersister(uintptr_t h) : handle(h) {
    spdlog::info("GoPersister::GoPersister handle {} {}", handle, fmt::ptr(this));
}

std::string GoPersister::loadBuffer() {
    constexpr int bufferSize = 8 * 1024 * 1024;
    std::string buffer(bufferSize, 0);
    int len = persister_go_read_callback(handle, buffer.data(), bufferSize);
    if (len < 0) {
        spdlog::error("GoPersister::loadBuffer: failed to read from Go persister");
        return {};
    }
    buffer.resize(len);
    return buffer;
}

raft::PersistentState GoPersister::load() {
    auto buffer = loadBuffer();
    if (buffer.empty()) {
        return raft::PersistentState{};
    }
    raft::proto::PersistentState p;
    if (p.ParseFromString(buffer)) {
        raft::PersistentState state;
        state.currentTerm = p.currentterm();
        if (p.has_votedfor()) {
            state.votedFor = p.votedfor();
        }
        for (int i = 0; i < p.log_size(); ++i) {
            const auto& entry = p.log(i);
            state.log.append(raft::LogEntry{entry.index(), entry.term(), zdb::commandFactory(entry.command())});
        }
        state.snapshotData = p.snapshot();
        state.lastIncludedIndex = p.lastincludedindex();
        state.lastIncludedTerm = p.lastincludedterm();
        return state;
    }
    spdlog::error("Failed to parse PersistentState from string");
    return raft::PersistentState{};
}

void GoPersister::save(const raft::PersistentState& s) {
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
    if (persister_go_invoke_callback(handle, str.data(), str.size()) != 0) {
        spdlog::error("GoPersister::save: failed to save to Go persister");
    }
}

GoPersister::~GoPersister() = default;
