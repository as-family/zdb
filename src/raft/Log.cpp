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

void Log::append(const proto::LogEntry& entry) {
    std::lock_guard g{m};
    entries.emplace_back(LogEntry {
        entry.index(),
        entry.term(),
        entry.command()
    });
}

void Log::append(const LogEntry& entry) {
    std::lock_guard g{m};
    entries.push_back(entry);
}

void Log::merge(const Log& other) {
    if (this == &other) return;
    std::scoped_lock lk(m, other.m);
    auto e = std::find_first_of(entries.begin(), entries.end(), other.entries.begin(), other.entries.end());
    if (e != entries.end()) {
        entries.erase(e, entries.end());
    }
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
    auto i = std::find_if(entries.begin(), entries.end(), [start](const LogEntry& e) { return e.index == start; });
    if (i == entries.end()) {
        return Log {};
    }
    auto es = std::vector<LogEntry>(i, entries.end());
    return Log {es};
}

std::optional<LogEntry> Log::at(uint64_t index) const {
    std::lock_guard g{m};
    auto i = std::find_if(entries.begin(), entries.end(), [index](const LogEntry& e) { return e.index == index; });
    if (i == entries.end()) {
        return std::nullopt;
    }
    return *i;
}
const std::vector<LogEntry> Log::data() const {
    std::lock_guard g{m};
    return entries;
}

} // namespace raft
