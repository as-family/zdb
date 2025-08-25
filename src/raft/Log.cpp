#include "raft/Log.hpp"
#include <proto/raft.pb.h>
#include <algorithm>
#include <ranges>

namespace raft {

LogEntry::~LogEntry() {
    delete command;
}

bool LogEntry::operator==(const LogEntry& other) const {
    return index == other.index && term == other.term;
}

Log::Log(Command* (* c)(const std::string&))
    : commandFactory {c} {}

LogEntry* Log::append(const proto::LogEntry& entry) {
    Command* cmd = commandFactory(entry.command());
    entries.emplace_back(LogEntry {
        entry.index(),
        entry.term(),
        cmd
    });
    return &entries.back();
}

void Log::append(const LogEntry& entry) {
    entries.push_back(entry);
}

void Log::merge(const Log& other) {
    auto e = std::find_first_of(entries.rbegin(), entries.rend(), other.entries.begin(), other.entries.end());
    if (e != entries.rend()) {
        entries.erase(e.base() - 1, entries.end());
    }
    entries.insert(entries.end(), other.entries.begin(), other.entries.end());
}

uint64_t Log::lastIndex() const {
    return entries.empty() ? 0 : entries.back().index;
}
uint64_t Log::lastTerm() const {
    return entries.empty() ? 0 : entries.back().term;
}

const std::vector<LogEntry> Log::suffix(uint64_t start) const {
    auto i = std::find_if(entries.begin(), entries.end(), [start](const LogEntry& e) { return e.index == start; });
    if (i == entries.end()) {
        return {};
    }
    return std::vector<LogEntry>(i, entries.end());
}

} // namespace raft
