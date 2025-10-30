// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * ZDB a distributed, fault-tolerant database.
 * Copyright (C) 2025 Ahmed Refaat Gadalla Mohamed
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "common/Command.hpp"
#include "proto/types.pb.h"
#include <google/protobuf/any.pb.h>
#include <memory>
#include "common/KVStateMachine.hpp"
namespace zdb
{

std::shared_ptr<raft::Command> commandFactory(const std::string& s) {
    auto cmd = zdb::proto::Command {};
    if (!cmd.ParseFromString(s)) {
        throw std::invalid_argument{"commandFactory: deserialization failed"};
    }
    if (cmd.op() == "g") {
        return std::make_shared<Get>(cmd);
    }
    if (cmd.op() == "s") {
        return std::make_shared<Set>(cmd);
    }
    if (cmd.op() == "e") {
        return std::make_shared<Erase>(cmd);
    }
    if (cmd.op() == "z") {
        return std::make_shared<Size>(cmd);
    }
    if (cmd.op() == "t") {
        return std::make_shared<TestCommand>(cmd);
    }
    if (cmd.op() == "n") {
        return std::make_shared<NoOp>(cmd);
    }
    if (cmd.op() == "i") {
        return std::make_shared<InstallSnapshotCommand>(cmd);
    }
    throw std::invalid_argument{"commandFactory: unknown command"};
}


Get::Get(UUIDV7& u, const Key& k) : key(k) {
    uuid = u;
}

Get::Get(const proto::Command& cmd) : key{cmd.key()} {
    uuid = string_to_uuid_v7(cmd.requestid().uuid());
    index = cmd.index();
}

std::string Get::serialize() const {
    auto c = proto::Command {};
    c.set_op("g");
    c.mutable_key()->set_data(key.data);
    c.mutable_requestid()->set_uuid(uuid_v7_to_string(uuid));
    c.set_index(index);
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
    if (const auto o = dynamic_cast<const Get*>(&other)) {
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
    index = cmd.index();
}

std::string Set::serialize() const {
    auto c = proto::Command {};
    c.set_op("s");
    c.mutable_key()->set_data(key.data);
    c.mutable_value()->set_data(value.data);
    c.mutable_value()->set_version(value.version);
    c.mutable_requestid()->set_uuid(uuid_v7_to_string(uuid));
    c.set_index(index);
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
    if (const auto o = dynamic_cast<const Set*>(&other)) {
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
    index = cmd.index();
}

std::string Erase::serialize() const {
    auto c = proto::Command {};
    c.set_op("e");
    c.mutable_key()->set_data(key.data);
    c.mutable_requestid()->set_uuid(uuid_v7_to_string(uuid));
    c.set_index(index);
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
    if (const auto o = dynamic_cast<const Erase*>(&other)) {
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
    index = cmd.index();
}

std::string Size::serialize() const {
    auto c = proto::Command {};
    c.set_op("z");
    c.mutable_requestid()->set_uuid(uuid_v7_to_string(uuid));
    c.set_index(index);
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

TestCommand::TestCommand(UUIDV7& u, std::string d) :data{d} {
    uuid = u;
}

TestCommand::TestCommand(const zdb::proto::Command& cmd) {
    data = cmd.key().data();
    index = cmd.index();
    uuid = string_to_uuid_v7(cmd.requestid().uuid());
}

std::string TestCommand::serialize() const {
    auto c = zdb::proto::Command {};
    c.set_op("t");
    c.mutable_key()->set_data(data);
    c.set_index(index);
    c.mutable_requestid()->set_uuid(uuid_v7_to_string(uuid));
    std::string s;
    if (!c.SerializeToString(&s)) {
        throw std::runtime_error("failed to serialize TestCommand command");
    }
    return s;
}

std::unique_ptr<raft::State> TestCommand::apply(raft::StateMachine& stateMachine) {
    return std::make_unique<zdb::State>(index);
}

bool TestCommand::operator==(const raft::Command& other) const {
    if (const auto o = dynamic_cast<const TestCommand*>(&other)) {
        return data == o->data;
    }
    return false;
}

bool TestCommand::operator!=(const raft::Command& other) const {
    return !(*this == other);
}

NoOp::NoOp(UUIDV7& u) {
    uuid = u;
}

NoOp::NoOp(const proto::Command& cmd) {
    uuid = string_to_uuid_v7(cmd.requestid().uuid());
    index = cmd.index();
}

std::string NoOp::serialize() const {
    auto c = proto::Command {};
    c.set_op("n");
    c.set_index(index);
    c.mutable_requestid()->set_uuid(uuid_v7_to_string(uuid));
    std::string s;
    if (!c.SerializeToString(&s)) {
        throw std::runtime_error("failed to serialize NoOp command");
    }
    return s;
}

std::unique_ptr<raft::State> NoOp::apply(raft::StateMachine& stateMachine) {
    return std::make_unique<zdb::State>(0L);
}

bool NoOp::operator==(const raft::Command& other) const {
    if (dynamic_cast<const NoOp*>(&other)) {
        return true;
    }
    return false;
}

bool NoOp::operator!=(const raft::Command& other) const {
    return !(*this == other);
}

InstallSnapshotCommand::InstallSnapshotCommand(UUIDV7& u, uint64_t lii, uint64_t lit, const std::string& d)
    : lastIncludedIndex{lii}, lastIncludedTerm{lit}, data{d} {
        uuid = u;
    }

InstallSnapshotCommand::InstallSnapshotCommand(const proto::Command& cmd)
    : lastIncludedIndex{cmd.index()}, lastIncludedTerm{cmd.value().version()}, data{cmd.value().data()} {
        uuid = string_to_uuid_v7(cmd.requestid().uuid());
        index = cmd.index();
    }

std::string InstallSnapshotCommand::serialize() const {
    auto c = proto::Command {};
    c.set_op("i");
    c.set_index(lastIncludedIndex);
    c.mutable_value()->set_version(lastIncludedTerm);
    c.mutable_value()->set_data(data);
    c.mutable_requestid()->set_uuid(uuid_v7_to_string(uuid));
    std::string s;
    if (!c.SerializeToString(&s)) {
        throw std::runtime_error("failed to serialize InstallSnapshotCommand command");
    }
    return s;
}

std::unique_ptr<raft::State> InstallSnapshotCommand::apply(raft::StateMachine& stateMachine) {
    auto& kvState = dynamic_cast<zdb::KVStateMachine&>(stateMachine);
    kvState.installSnapshot(lastIncludedIndex, lastIncludedTerm, data);
    return std::make_unique<zdb::State>(0L);
}

bool InstallSnapshotCommand::operator==(const raft::Command& other) const {
    if (const auto o = dynamic_cast<const InstallSnapshotCommand*>(&other)) {
        return lastIncludedIndex == o->lastIncludedIndex &&
               lastIncludedTerm == o->lastIncludedTerm &&
               data == o->data;
    }
    return false;
}

bool InstallSnapshotCommand::operator!=(const raft::Command& other) const {
    return !(*this == other);
}

} // namespace zdb
