#ifndef RAFT_STATE_MACHINE_H
#define RAFT_STATE_MACHINE_H

#include "raft/Log.hpp"
#include "raft/Command.hpp"
#include <unordered_map>
#include <string>

namespace raft {

class Command;

struct State {
};

class StateMachine {
public:
    virtual ~StateMachine() = default;

    virtual void applyCommand(raft::Command& command) = 0;
    virtual void consumeChannel() = 0;
    virtual void snapshot() = 0;
    virtual void restore(const std::string& snapshot) = 0;
};

} // namespace raft

#endif // RAFT_STATE_MACHINE_H
