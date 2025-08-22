#ifndef RAFT_COMMAND_H
#define RAFT_COMMAND_H

namespace raft {

class Command {
    virtual ~Command() = default;
    virtual void apply() = 0;
};

} // namespace raft

#endif // RAFT_COMMAND_H
