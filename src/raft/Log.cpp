#include "raft/Log.hpp"

namespace raft {

uint64_t Log::lastIndex() const {
    return entries.empty() ? 0 : entries.size();
}
uint64_t Log::lastTerm() const {
    return entries.empty() ? 0 : entries.back().term;
}

} // namespace raft
