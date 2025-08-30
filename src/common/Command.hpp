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

    Get(const Key& k)
        : key(k) {}
    Get(const proto::Command& cmd)
        :key {cmd.key()} {
    }

    std::string serialize() const override {
        auto c = proto::Command {};
        c.set_op("get");
        c.mutable_key()->set_data(key.data);
        auto s = c.SerializeAsString();
        if (s == "") {
            throw std::runtime_error("failed to serialize Get command");
        }
        return s;
    }

    raft::State* apply(raft::StateMachine* stateMachine) override {
        auto* kvState = dynamic_cast<zdb::KVStateMachine*>(stateMachine);
        if (kvState) {
            return kvState->get(key);
        }
        return nullptr;
    }
    bool operator==(const raft::Command& other) const override {
        if (auto o = dynamic_cast<const Get*>(&other)) {
            return key == o->key;
        }
        return false;
    }

    bool operator!=(const raft::Command& other) const override {
        return !(*this == other);
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
        c.set_op("set");
        c.mutable_key()->set_data(key.data);
        c.mutable_value()->set_data(value.data);
        c.mutable_value()->set_version(value.version);
        auto s = c.SerializeAsString();
        if (s == "") {
            throw std::runtime_error("failed to serialize Set command");
        }
        return s;
    }

    raft::State* apply(raft::StateMachine* stateMachine) override {
        auto* kvState = dynamic_cast<zdb::KVStateMachine*>(stateMachine);
        if (kvState) {
            return kvState->set(key, value);
        }
        return nullptr;
    }

    bool operator==(const raft::Command& other) const override {
        if (auto o = dynamic_cast<const Set*>(&other)) {
            return key == o->key && value == o->value;
        }
        return false;
    }

    bool operator!=(const raft::Command& other) const override {
        return !(*this == other);
    }

};

struct Erase : public raft::Command {
    Key key;

    Erase(const Key& k)
        : key(k) {}
    Erase(const proto::Command& cmd)
        : key {cmd.key()} {}

    std::string serialize() const override {
        auto c = proto::Command {};
        c.set_op("erase");
        c.mutable_key()->set_data(key.data);
        auto s = c.SerializeAsString();
        if (s == "") {
            throw std::runtime_error("failed to serialize Erase command");
        }
        return s;
    }

    raft::State* apply(raft::StateMachine* stateMachine) override {
        auto* kvState = dynamic_cast<zdb::KVStateMachine*>(stateMachine);
        if (kvState) {
            return kvState->erase(key);
        }
        return nullptr;
    }

    bool operator==(const raft::Command& other) const override {
        if (auto o = dynamic_cast<const Erase*>(&other)) {
            return key == o->key;
        }
        return false;
    }

    bool operator!=(const raft::Command& other) const override {
        return !(*this == other);
    }

};

struct Size : public raft::Command {
    Size() {}
    Size(const proto::Command& cmd) {}

    std::string serialize() const override {
        auto c = proto::Command {};
        c.set_op("size");
        auto s = c.SerializeAsString();
        if (s == "") {
            throw std::runtime_error("failed to serialize Size command");
        }
        return s;
    }

    raft::State* apply(raft::StateMachine* stateMachine) override {
        auto* kvState = dynamic_cast<zdb::KVStateMachine*>(stateMachine);
        if (kvState) {
            return kvState->size();
        }
        return nullptr;
    }

    bool operator==(const raft::Command& other) const override {
        if (auto o = dynamic_cast<const Size*>(&other)) {
            return true;
        }
        return false;
    }

    bool operator!=(const raft::Command& other) const override {
        return !(*this == other);
    }

};

raft::Command* commandFactory(const std::string& s);

} // namespace zdb

#endif // COMMAND_H
