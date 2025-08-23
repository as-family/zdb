#ifndef RAFT_LOG_H
#define RAFT_LOG_H

#include <cstdint>
#include <vector>
#include "raft/Command.hpp"
#include <proto/raft.pb.h>

namespace raft {

struct LogEntry {
    uint64_t index;
    uint64_t term;
    Command* command;
    LogEntry(const proto::LogEntry& entry);
};

class Log {
public:
    uint64_t lastIndex() const;
    uint64_t lastTerm() const;
    std::vector<LogEntry> entries;
};

} // namespace raft

#endif // RAFT_LOG_H
