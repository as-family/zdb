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
#ifndef RAFT_LOG_H
#define RAFT_LOG_H

#include <cstdint>
#include <vector>
#include "raft/Command.hpp"
#include <proto/raft.pb.h>
#include <functional>
#include <optional>
#include <mutex>
#include <memory>

namespace raft {

struct LogEntry {
    LogEntry(uint64_t idx, uint64_t t, std::shared_ptr<Command> cmd)
    : index(idx), term(t), command(cmd) {}

    uint64_t index;
    uint64_t term;
    std::shared_ptr<Command> command;
    bool operator==(const LogEntry& other) const;
};

class Log {
public:
    Log();
    Log(std::vector<LogEntry> es);
    uint64_t lastIndex() const;
    uint64_t lastTerm() const;
    uint64_t firstIndex() const;
    uint64_t firstTerm() const;
    uint64_t termFirstIndex(uint64_t term) const;
    uint64_t termLastIndex(uint64_t term) const;
    void append(const LogEntry& entry);
    void merge(const Log& other);
    void clear();
    std::optional<LogEntry> at(uint64_t index) const;
    Log suffix(uint64_t start) const;
    void trimPrefix(uint64_t index, u_int64_t term);
    void setLastIncluded(uint64_t index, uint64_t term);
    std::vector<LogEntry> data() const;
 private:
    mutable std::mutex m{};
    uint64_t lastIncludedIndex = 0;
    uint64_t lastIncludedTerm = 0;
    std::vector<LogEntry> entries;
    std::optional<std::vector<LogEntry>::const_iterator> atIter(uint64_t index) const;
};

} // namespace raft

#endif // RAFT_LOG_H
