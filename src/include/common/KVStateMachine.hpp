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
#ifndef KV_STATEMACHINE_H
#define KV_STATEMACHINE_H

#include "raft/StateMachine.hpp"
#include "common/Types.hpp"
#include "raft/Channel.hpp"
#include <thread>
#include "storage/StorageEngine.hpp"
#include "raft/Raft.hpp"
#include <chrono>
#include <memory>
#include <mutex>

namespace zdb {

class KVStateMachine : public raft::StateMachine {
public:
    KVStateMachine(StorageEngine& s,  raft::Channel<std::shared_ptr<raft::Command>>& raftCh, raft::Raft& r);
    KVStateMachine(const KVStateMachine&) = delete;
    KVStateMachine& operator=(const KVStateMachine&) = delete;
    KVStateMachine(KVStateMachine&&) = delete;
    KVStateMachine& operator=(KVStateMachine&&) = delete;
    std::unique_ptr<raft::State> applyCommand(raft::Command& command) override;
    void consumeChannel() override;
    void snapshot() override;
    void restore(const std::string& snapshot) override;
    std::unique_ptr<raft::State> handle(std::shared_ptr<raft::Command>, std::chrono::system_clock::time_point t);
    State get(Key key);
    State set(Key key, Value value);
    State erase(Key key);
    State size();
    ~KVStateMachine() override;
private:
    std::mutex m;
    StorageEngine& storageEngine;
    raft::Channel<std::shared_ptr<raft::Command>>& raftChannel;
    raft::Raft& raft;
    std::thread consumerThread;
};

} // namespace zdb

#endif // KV_STATEMACHINE_H
