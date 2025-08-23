#ifndef RAFT_COMMAND_H
#define RAFT_COMMAND_H

#include <string>

namespace raft {

struct Command {
    virtual ~Command() = default;
    virtual void apply() = 0;
    virtual std::string serialize() const = 0;
};

} // namespace raft

#endif // RAFT_COMMAND_H
