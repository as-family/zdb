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
#include "interface/StorageEngine.hpp"
#include "raft/Raft.hpp"
#include <chrono>
#include "common/Command.hpp"
#include <memory>

namespace zdb {

class KVStateMachine : public raft::StateMachine {
public:
    KVStateMachine(StorageEngine& s,  raft::Channel& leaderChannel, raft::Channel& followerChannel, raft::Raft& r);
    KVStateMachine(const KVStateMachine&) = delete;
    KVStateMachine& operator=(const KVStateMachine&) = delete;
    KVStateMachine(KVStateMachine&&) = delete;
    KVStateMachine& operator=(KVStateMachine&&) = delete;
    std::unique_ptr<raft::State> applyCommand(raft::Command& command) override;
    void consumeChannel() override;
    void snapshot() override;
    void restore(const std::string& snapshot) override;
    std::unique_ptr<raft::State> handleGet(Get c, std::chrono::system_clock::time_point t);
    std::unique_ptr<raft::State> handleSet(Set c, std::chrono::system_clock::time_point t);
    std::unique_ptr<raft::State> handleErase(Erase c, std::chrono::system_clock::time_point t);
    std::unique_ptr<raft::State> handleSize(Size c, std::chrono::system_clock::time_point t);
    State get(Key key);
    State set(Key key, Value value);
    State erase(Key key);
    State size();
    ~KVStateMachine();
private:
    StorageEngine& storageEngine;
    raft::Channel& leader;
    raft::Channel& follower;
    raft::Raft& raft;
    std::thread consumerThread;
};

} // namespace zdb

#endif // KV_STATEMACHINE_H
