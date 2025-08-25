#ifndef RAFT_COMMAND_H
#define RAFT_COMMAND_H

#include <string>

namespace raft {

class StateMachine;

struct Command {
    virtual ~Command() = default;
    virtual std::string serialize() const = 0;
    virtual void apply(raft::StateMachine* stateMachine) = 0;
};

} // namespace raft

#endif // RAFT_COMMAND_H
