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
#ifndef COMMAND_H
#define COMMAND_H

#include "raft/Command.hpp"
#include "raft/StateMachine.hpp"
#include "common/Types.hpp"
#include "proto/types.pb.h"
#include <string>
#include <memory>
#include "common/Util.hpp"

namespace zdb {

struct Get : public raft::Command {
    Key key;

    Get(UUIDV7& u, const Key& k);
    Get(const proto::Command& cmd);

    std::string serialize() const override;

    std::unique_ptr<raft::State> apply(raft::StateMachine& stateMachine) override;
    bool operator==(const raft::Command& other) const override;
    bool operator!=(const raft::Command& other) const override;
};

struct Set : public raft::Command {
    Key key;
    Value value;

    Set(UUIDV7& u, const Key& k, const Value& v);
    Set(const proto::Command& cmd);

    std::string serialize() const override;

    std::unique_ptr<raft::State> apply(raft::StateMachine& stateMachine) override;

    bool operator==(const raft::Command& other) const override;
    bool operator!=(const raft::Command& other) const override;

};

struct Erase : public raft::Command {
    Key key;

    Erase(UUIDV7& u, const Key& k);
    Erase(const proto::Command& cmd);

    std::string serialize() const override;

    std::unique_ptr<raft::State> apply(raft::StateMachine& stateMachine) override;

    bool operator==(const raft::Command& other) const override;
    bool operator!=(const raft::Command& other) const override;

};

struct Size : public raft::Command {
    Size(UUIDV7& u);
    Size(const proto::Command&);

    std::string serialize() const override;

    std::unique_ptr<raft::State> apply(raft::StateMachine& stateMachine) override;

    bool operator==(const raft::Command& other) const override;
    bool operator!=(const raft::Command& other) const override;
};

std::shared_ptr<raft::Command> commandFactory(const std::string& s);

} // namespace zdb

#endif // COMMAND_H
