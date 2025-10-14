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

#ifndef GO_CHANNEL_HPP
#define GO_CHANNEL_HPP

#include "raft/Channel.hpp"
#include <raft/Command.hpp>
#include "raft_wrapper.hpp"

class GoChannel : public raft::Channel<std::shared_ptr<raft::Command>> {
public:
    GoChannel(uintptr_t h, RaftHandle* r);
    ~GoChannel() override;
    void send(std::shared_ptr<raft::Command>) override;
    bool sendUntil(std::shared_ptr<raft::Command>, std::chrono::system_clock::time_point t) override;
    std::optional<std::shared_ptr<raft::Command>> receive() override;
    std::optional<std::shared_ptr<raft::Command>> receiveUntil(std::chrono::system_clock::time_point t) override;
    void close() override;
    bool isClosed() override;
private:
    uintptr_t handle;
    RaftHandle* raftHandle;
};

#endif // GO_CHANNEL_HPP
