#ifndef RAFT_COMMAND_H
#define RAFT_COMMAND_H

#include <string>
#include "common/Util.hpp"

namespace raft {

class StateMachine;
class State;

struct Command {
    virtual ~Command() = default;
    virtual std::string serialize() const = 0;
    virtual void apply(raft::StateMachine& stateMachine) = 0;
    virtual bool operator==(const Command& other) const = 0;
    virtual bool operator!=(const Command& other) const = 0;
    virtual UUIDV7 getUUID() const {
        return uuid;
    };
protected:
    UUIDV7 uuid;
};

} // namespace raft

#endif // RAFT_COMMAND_H
