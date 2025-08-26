#ifndef RAFT_LOG_H
#define RAFT_LOG_H

#include <cstdint>
#include <vector>
#include "raft/Command.hpp"
#include <proto/raft.pb.h>
#include <functional>
#include <optional>
#include <mutex>

namespace raft {

struct LogEntry {
    uint64_t index;
    uint64_t term;
    Command* command;
    ~LogEntry();
    bool operator==(const LogEntry& other) const;
};

class Log {
public:
    Log(Command* (* c)(const std::string&));
    Log(Command* (* c)(const std::string&), std::vector<LogEntry>& es);
    uint64_t lastIndex() const;
    uint64_t lastTerm() const;
    uint64_t firstIndex() const;
    uint64_t firstTerm() const;
    LogEntry* append(const proto::LogEntry& entry);
    void append(const LogEntry& entry);
    void merge(const Log& other);
    std::optional<LogEntry> at(uint64_t index);
    Log suffix(uint64_t start) const;
    std::vector<LogEntry> entries;
private:
    Command* (*commandFactory)(const std::string&);
    mutable std::mutex m{}; 
};

} // namespace raft

#endif // RAFT_LOG_H
