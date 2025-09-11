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
#include "raft/Log.hpp"
#include <proto/raft.pb.h>
#include <algorithm>
#include <ranges>
#include <optional>
#include <mutex>

namespace raft {

bool LogEntry::operator==(const LogEntry& other) const {
    return index == other.index && term == other.term;
}

Log::Log()
    : entries{} {}

Log::Log(std::vector<LogEntry> es)
    : entries{es} {}

void Log::append(const LogEntry& entry) {
    std::lock_guard g{m};
    entries.push_back(entry);
}

void Log::merge(const Log& other) {
    if (this == &other) return;
    std::scoped_lock lk(m, other.m);
    if (other.entries.empty()) {
        return;
    }
    const auto i = other.entries.front().index;
    auto cut = std::ranges::find_if(entries, [i](const LogEntry& e) { return e.index >= i; });
    if (cut == entries.end()) {
        entries.insert(entries.end(), other.entries.begin(), other.entries.end());
        return;
    }
    if (cut->index == i && cut->term == other.entries.front().term) {
        ++cut;
        entries.erase(cut, entries.end());
        entries.insert(entries.end(), std::next(other.entries.begin()), other.entries.end());
        return;
    }
    entries.erase(cut, entries.end());
    entries.insert(entries.end(), other.entries.begin(), other.entries.end()); 
}

uint64_t Log::lastIndex() const {
    std::lock_guard g{m};
    return entries.empty() ? 0 : entries.back().index;
}
uint64_t Log::lastTerm() const {
    std::lock_guard g{m};
    return entries.empty() ? 0 : entries.back().term;
}

uint64_t Log::firstIndex() const {
    std::lock_guard g{m};
    return entries.empty() ? 0 : entries.front().index;
}
uint64_t Log::firstTerm() const {
    std::lock_guard g{m};
    return entries.empty() ? 0 : entries.front().term;
}

Log Log::suffix(uint64_t start) const {
    std::lock_guard g{m};
    auto i = std::ranges::find_if(entries, [start](const LogEntry& e) { return e.index == start; });
    if (i == entries.end()) {
        return Log {};
    }
    auto es = std::vector<LogEntry>(i, entries.end());
    return Log {es};
}

std::optional<LogEntry> Log::at(uint64_t index) const {
    std::lock_guard g{m};
    auto i = std::ranges::find_if(entries, [index](const LogEntry& e) { return e.index == index; });
    if (i == entries.end()) {
        return std::nullopt;
    }
    return *i;
}

std::vector<LogEntry> Log::data() const {
    std::lock_guard g{m};
    return entries;
}

} // namespace raft
