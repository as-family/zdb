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

#ifndef GO_STATE_MACHINE_H
#define GO_STATE_MACHINE_H

#include <raft/StateMachine.hpp>
#include <raft/Command.hpp>
#include <raft/Types.hpp>
#include <memory>
#include <cstdint>

extern "C" int state_machine_go_apply_command(uintptr_t handle, void *command, int command_size, void* state);
extern "C" int state_machine_go_snapshot(uintptr_t handle, void* buffer, int buffer_len);
extern "C" int state_machine_go_install_snapshot(uintptr_t handle, void* buffer, int buffer_len);

class GoStateMachine : public raft::StateMachine {
public:
    GoStateMachine(uintptr_t h);
    std::unique_ptr<raft::State> applyCommand(raft::Command& command) override;
    std::shared_ptr<raft::Command> snapshot() override;
    void installSnapshot(std::shared_ptr<raft::Command>) override;
private:
    uintptr_t handle;
};

#endif // GO_STATE_MACHINE_H
