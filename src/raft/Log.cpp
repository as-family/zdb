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

    // Find where to start merging - first entry in other log
    const auto start_index = other.entries.front().index;
    auto our_it = std::ranges::find_if(entries, [start_index](const LogEntry& e) {
        return e.index >= start_index;
    });

    // If we don't have any entries at or after start_index, just append everything
    if (our_it == entries.end()) {
        entries.insert(entries.end(), other.entries.begin(), other.entries.end());
        return;
    }

    // Compare entries one by one to find first conflict
    auto other_it = other.entries.begin();
    auto conflict_point = our_it;

    while (our_it != entries.end() && other_it != other.entries.end() &&
           our_it->index == other_it->index) {

        // If terms differ, we have a conflict - truncate from here
        if (our_it->term != other_it->term) {
            break;
        }

        // Terms match, move to next entry
        ++our_it;
        ++other_it;
        conflict_point = our_it;
    }

    // Truncate our log from the conflict point and append remaining other entries
    entries.erase(conflict_point, entries.end());
    entries.insert(entries.end(), other_it, other.entries.end());
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
    std::vector<LogEntry> es{i, entries.end()};
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
