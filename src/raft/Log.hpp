#ifndef RAFT_LOG_H
#define RAFT_LOG_H

#include <cstdint>
#include <vector>
#include "raft/Command.hpp"
#include <proto/raft.pb.h>
#include <functional>

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
    uint64_t lastIndex() const;
    uint64_t lastTerm() const;
    LogEntry* append(const proto::LogEntry& entry);
    void merge(const Log& other);
    const std::vector<LogEntry> suffix(uint64_t start) const;
private:
    Command* (*commandFactory)(const std::string&);
    std::vector<LogEntry> entries;
};

} // namespace raft

#endif // RAFT_LOG_H
