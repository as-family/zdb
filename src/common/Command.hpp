#ifndef COMMAND_H
#define COMMAND_H

#include "raft/Command.hpp"
#include "raft/StateMachine.hpp"
#include "server/KVStoreServer.hpp"
#include "proto/types.pb.h"

namespace zdb {

struct Get : public raft::Command {
    Key key;

    Get(const Key& k)
        : key(k) {}
    Get(const proto::Command& cmd)
        :key {cmd.key()} {
    }

    std::string serialize() const override {
        auto c = proto::Command {};
        c.set_op("get");
        c.mutable_key()->set_data(key.data);
        return c.SerializeAsString();
    }

    raft::State* apply(raft::StateMachine* stateMachine) override {
        auto* kvState = dynamic_cast<zdb::KVStoreServiceImpl*>(stateMachine);
        if (kvState) {
            return kvState->handleGet(key);
        }
        return nullptr;
    }
};

struct Set : public raft::Command {
    Key key;
    Value value;

    Set(const Key& k, const Value& v)
        : key(k), value(v) {}
    Set(const proto::Command& cmd)
        : key {cmd.key()}, value {cmd.value()} {}

    std::string serialize() const override {
        auto c = proto::Command {};
        c.set_op("put");
        c.mutable_key()->set_data(key.data);
        c.mutable_value()->set_data(value.data);
        return c.SerializeAsString();
    }

    raft::State* apply(raft::StateMachine* stateMachine) override {
        auto* kvState = dynamic_cast<zdb::KVStoreServiceImpl*>(stateMachine);
        if (kvState) {
            return kvState->handleSet(key, value);
        }
        return nullptr;
    }
};

} // namespace zdb

#endif // COMMAND_H
