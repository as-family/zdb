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

#ifndef RAFT_RSM_H
#define RAFT_RSM_H

#include <raft/StateMachine.hpp>
#include <memory>
#include <raft/Channel.hpp>
#include <raft/Raft.hpp>
#include <mutex>
#include <thread>
#include <raft/Types.hpp>

namespace raft {

class Rsm {
public:
    Rsm(std::shared_ptr<StateMachine> m, std::shared_ptr<raft::Channel<std::shared_ptr<raft::Command>>> rCh, std::shared_ptr<raft::Raft> r);
    void consumeChannel();
    std::unique_ptr<raft::State> handle(std::shared_ptr<raft::Command> c, std::chrono::system_clock::time_point t);
    void asyncConsume();
    void syncHandle();
    ~Rsm();

private:
    std::mutex m;
    bool consume = true;
    std::shared_ptr<raft::Channel<std::shared_ptr<raft::Command>>> raftCh;
    std::shared_ptr<raft::Raft> raft;
    std::shared_ptr<StateMachine> machine;
    std::thread consumerThread;
};

} // namespace raft

#endif // RAFT_RSM_H
