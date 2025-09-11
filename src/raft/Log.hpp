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
    LogEntry(uint64_t idx, uint64_t t, std::unique_ptr<Command> cmd)
    : index(idx), term(t), command(std::move(cmd)) {}

    LogEntry(const LogEntry& other)
    : index(other.index), term(other.term),
      command(other.command ? other.command->clone() : nullptr) {}

    LogEntry(LogEntry&& other) noexcept
    : index(other.index), term(other.term), command(std::move(other.command)) {}

    LogEntry& operator=(const LogEntry& other) {
        if (this != &other) {
            index = other.index;
            term = other.term;
            command = other.command ? other.command->clone() : nullptr;
        }
        return *this;
    }

    LogEntry& operator=(LogEntry&& other) noexcept {
        if (this != &other) {
            index = other.index;
            term = other.term;
            command = std::move(other.command);
        }
        return *this;
    }

    uint64_t index;
    uint64_t term;
    std::unique_ptr<Command> command;
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
    void append(const LogEntry& entry);
    void merge(const Log& other);
    std::optional<LogEntry> at(uint64_t index) const;
    Log suffix(uint64_t start) const;
    std::vector<LogEntry> data() const;
 private:
    mutable std::mutex m{};
    std::vector<LogEntry> entries;
};

} // namespace raft

#endif // RAFT_LOG_H
