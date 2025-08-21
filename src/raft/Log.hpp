#ifndef RAFT_LOG_H
#define RAFT_LOG_H

#include <cstdint>
#include <vector>

namespace raft {

class Command {
    virtual ~Command() = default;
    virtual void apply() = 0;
};

class LogEntry {
    uint64_t term;
    Command* command;
};

class Log {
    std::vector<LogEntry> entries;
};

} // namespace raft

#endif // RAFT_LOG_H
