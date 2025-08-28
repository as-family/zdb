#ifndef RAFT_COMMAND_H
#define RAFT_COMMAND_H

#include <string>

namespace raft {

class StateMachine;
class State;

struct Command {
    virtual ~Command() = default;
    virtual std::string serialize() const = 0;
    virtual State* apply(raft::StateMachine* stateMachine) = 0;
};

} // namespace raft

#endif // RAFT_COMMAND_H
