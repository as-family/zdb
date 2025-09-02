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
    std::string command;
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
    void append(const proto::LogEntry& entry);
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
