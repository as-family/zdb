#include "common/Command.hpp"
#include "proto/types.pb.h"
#include <google/protobuf/any.pb.h>
#include <memory>
#include "common/KVStateMachine.hpp"

namespace zdb
{

std::unique_ptr<raft::Command> commandFactory(const std::string& s) {
    auto cmd = zdb::proto::Command {};
    if (!cmd.ParseFromString(s)) {
        throw std::invalid_argument{"commandFactory: deserialization failed"};
    }
    if (cmd.op() == "get") {
        return std::make_unique<Get>(cmd);
    }
    if (cmd.op() == "set") {
        return std::make_unique<Set>(cmd);
    }
    if (cmd.op() == "erase") {
        return std::make_unique<Erase>(cmd);
    }
    if (cmd.op() == "size") {
        return std::make_unique<Size>(cmd);
    }
    throw std::invalid_argument{"commandFactory: unknown op: " + cmd.op()};
}


Get::Get(UUIDV7& u, const Key& k) : key(k) {
    uuid = u;
}

Get::Get(const proto::Command& cmd) : key{cmd.key()} {
    uuid = string_to_uuid_v7(cmd.requestid().uuid());
}

std::string Get::serialize() const {
    auto c = proto::Command {};
    c.set_op("get");
    c.mutable_key()->set_data(key.data);
    std::string s;
    if (!c.SerializeToString(&s)) {
        throw std::runtime_error("failed to serialize Get command");
    }
    return s;
}

std::unique_ptr<raft::State> Get::apply(raft::StateMachine& stateMachine) {
    auto& kvState = dynamic_cast<zdb::KVStateMachine&>(stateMachine);
    return std::make_unique<State>(kvState.get(key));
}

bool Get::operator==(const raft::Command& other) const {
    if (auto o = dynamic_cast<const Get*>(&other)) {
        return key == o->key;
    }
    return false;
}

bool Get::operator!=(const raft::Command& other) const {
    return !(*this == other);
}

Set::Set(UUIDV7& u, const Key& k, const Value& v) : key(k), value(v) {
    uuid = u;
}

Set::Set(const proto::Command& cmd) : key{cmd.key()}, value{cmd.value()} {
    uuid = string_to_uuid_v7(cmd.requestid().uuid());
}

std::string Set::serialize() const {
    auto c = proto::Command {};
    c.set_op("set");
    c.mutable_key()->set_data(key.data);
    c.mutable_value()->set_data(value.data);
    c.mutable_value()->set_version(value.version);
    std::string s;
    if (!c.SerializeToString(&s)) {
        throw std::runtime_error("failed to serialize Set command");
    }
    return s;
}

std::unique_ptr<raft::State> Set::apply(raft::StateMachine& stateMachine) {
    auto& kvState = dynamic_cast<zdb::KVStateMachine&>(stateMachine);
    return std::make_unique<State>(kvState.set(key, value));
}

bool Set::operator==(const raft::Command& other) const {
    if (auto o = dynamic_cast<const Set*>(&other)) {
        return key == o->key && value == o->value;
    }
    return false;
}

bool Set::operator!=(const raft::Command& other) const {
    return !(*this == other);
}

Erase::Erase(UUIDV7& u, const Key& k) : key(k) {
    uuid = u;
}

Erase::Erase(const proto::Command& cmd) : key{cmd.key()} {
    uuid = string_to_uuid_v7(cmd.requestid().uuid());
}

std::string Erase::serialize() const {
    auto c = proto::Command {};
    c.set_op("erase");
    c.mutable_key()->set_data(key.data);
    std::string s;
    if (!c.SerializeToString(&s)) {
        throw std::runtime_error("failed to serialize Erase command");
    }
    return s;
}

std::unique_ptr<raft::State> Erase::apply(raft::StateMachine& stateMachine) {
    auto& kvState = dynamic_cast<zdb::KVStateMachine&>(stateMachine);
    return std::make_unique<State>(kvState.erase(key));
}

bool Erase::operator==(const raft::Command& other) const {
    if (auto o = dynamic_cast<const Erase*>(&other)) {
        return key == o->key;
    }
    return false;
}

bool Erase::operator!=(const raft::Command& other) const {
    return !(*this == other);
}

Size::Size(UUIDV7& u) {
    uuid = u;
}

Size::Size(const proto::Command& cmd) {
    uuid = string_to_uuid_v7(cmd.requestid().uuid());
}

std::string Size::serialize() const {
    auto c = proto::Command {};
    c.set_op("size");
    std::string s;
    if (!c.SerializeToString(&s)) {
        throw std::runtime_error("failed to serialize Size command");
    }
    return s;
}

std::unique_ptr<raft::State> Size::apply(raft::StateMachine& stateMachine) {
    auto& kvState = dynamic_cast<zdb::KVStateMachine&>(stateMachine);
    return std::make_unique<State>(kvState.size());
}

bool Size::operator==(const raft::Command& other) const {
    if (dynamic_cast<const Size*>(&other)) {
        return true;
    }
    return false;
}

bool Size::operator!=(const raft::Command& other) const {
    return !(*this == other);
}

} // namespace zdb
