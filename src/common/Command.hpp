#ifndef COMMAND_H
#define COMMAND_H

#include "raft/Command.hpp"
#include "raft/StateMachine.hpp"
#include "common/Types.hpp"
#include "proto/types.pb.h"
#include "common/KVStateMachine.hpp"

namespace zdb {

struct Get : public raft::Command {
    Key key;

    Get(const Key& k);
    Get(const proto::Command& cmd);

    std::string serialize() const override;

    void apply(raft::StateMachine& stateMachine) override;
    bool operator==(const raft::Command& other) const override;
    bool operator!=(const raft::Command& other) const override;

};

struct Set : public raft::Command {
    Key key;
    Value value;

    Set(const Key& k, const Value& v);
    Set(const proto::Command& cmd);

    std::string serialize() const override;

    void apply(raft::StateMachine& stateMachine) override;

    bool operator==(const raft::Command& other) const override;
    bool operator!=(const raft::Command& other) const override;

};

struct Erase : public raft::Command {
    Key key;

    Erase(const Key& k);
    Erase(const proto::Command& cmd);

    std::string serialize() const override;

    void apply(raft::StateMachine& stateMachine) override;

    bool operator==(const raft::Command& other) const override;
    bool operator!=(const raft::Command& other) const override;

};

struct Size : public raft::Command {
    Size();
    Size(const proto::Command&);

    std::string serialize() const override;

    void apply(raft::StateMachine& stateMachine) override;

    bool operator==(const raft::Command& other) const override;
    bool operator!=(const raft::Command& other) const override;

};

std::unique_ptr<raft::Command> commandFactory(const std::string& s);

} // namespace zdb

#endif // COMMAND_H
