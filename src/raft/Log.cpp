#include "raft/Log.hpp"
#include <proto/raft.pb.h>
#include "common/Types.hpp"

namespace raft {

LogEntry::LogEntry(const proto::LogEntry& entry)
    : term(entry.term()),
      command(new zdb::Command(entry.command())) {}

uint64_t Log::lastIndex() const {
    return entries.empty() ? 0 : entries.size();
}
uint64_t Log::lastTerm() const {
    return entries.empty() ? 0 : entries.back().term;
}

} // namespace raft
