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

#include <memory>
#include <raft/Rsm.hpp>
#include <common/Error.hpp>
#include <common/Types.hpp>
#include <spdlog/spdlog.h>
#include <mutex>
#include <future>

namespace raft {

Rsm::Rsm(StateMachine* m, raft::Channel<std::shared_ptr<raft::Command>>* rCh, raft::Raft* r)
    : raftCh {rCh}, raft {r}, machine {m}, consumerThread {&raft::Rsm::consumeChannel, this} {}

void Rsm::consumeChannel() {
    while (running) {
        auto c = raftCh->receiveUntil(std::chrono::system_clock::now() + std::chrono::milliseconds{1L});
        if (c.has_value()) {
            if (c.value().has_value()) {
                auto s = machine->applyCommand(*c.value().value());
                pending.complete(uuid_v7_to_string(c.value().value()->getUUID()), std::move(s));
            }
        } else {
            break;
        }
    }
}

std::unique_ptr<raft::State> Rsm::handle(std::shared_ptr<raft::Command> c, std::chrono::system_clock::time_point t) {
    if (!raft->start(c)) {
        return std::make_unique<zdb::State>(zdb::Error{zdb::ErrorCode::NotLeader, "not the leader"});
    }
    auto f = pending.add(uuid_v7_to_string(c->getUUID()));
    auto status = f.wait_for(std::chrono::seconds{2L});
    if (status == std::future_status::ready) {
        return f.get();
    }
    return std::make_unique<zdb::State>(zdb::Error{zdb::ErrorCode::NotLeader, "not the leader"});
    
    auto r = f.get();
    if (!r) {
        return std::make_unique<zdb::State>(zdb::Error{zdb::ErrorCode::Internal, "Channel closed unexpectedly"});
    }
    return r;
}

Rsm::~Rsm() {
    spdlog::info("~Rsm");
    running = false;
    pending.cancelAll();
    if (consumerThread.joinable()) {
        consumerThread.join();
    }
    pending.cancelAll();
    std::this_thread::sleep_for(std::chrono::seconds {1L});
    spdlog::info("Destroyed Rsm");
}


} // namespace raft
