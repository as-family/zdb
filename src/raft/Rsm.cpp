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

#include <raft/Rsm.hpp>
#include <common/Error.hpp>
#include <common/Types.hpp>
#include <spdlog/spdlog.h>

namespace raft {

Rsm::Rsm(std::shared_ptr<StateMachine> m, std::shared_ptr<raft::Channel<std::shared_ptr<raft::Command>>> rCh, std::shared_ptr<raft::Raft> r)
    : raftCh {rCh}, raft {r}, machine {m}, consumerThread {&raft::Rsm::consumeChannel, this} {}

void Rsm::consumeChannel() {
    while (true) {
        std::unique_lock lock{m};
        auto c = raftCh->receiveUntil(std::chrono::system_clock::now() + std::chrono::milliseconds{1L});
        if (c.has_value()) {
            if (c.value().has_value()) {
                machine->applyCommand(*c.value().value());
            }
        } else {
            break;
        }
    }
}

std::unique_ptr<raft::State> Rsm::handle(std::shared_ptr<raft::Command> c, std::chrono::system_clock::time_point t) {
    std::unique_lock lock{m};
    if (!raft->start(c)) {
        return std::make_unique<zdb::State>(zdb::Error{zdb::ErrorCode::NotLeader, "not the leader"});
    }
    while (true) {
        auto r = raftCh->receiveUntil(t);
        if (!r.has_value()) {
            return std::make_unique<zdb::State>(zdb::Error{zdb::ErrorCode::Internal, "Channel closed unexpectedly"});
        }
        if (!r.value().has_value()) {
            return std::make_unique<zdb::State>(zdb::Error{zdb::ErrorCode::Timeout, "request timed out"});
        }
        auto s = machine->applyCommand(*r.value().value());
        if (r.value().value()->getUUID() == c->getUUID()) {
            return s;
        }
    }
}

Rsm::~Rsm() {
    spdlog::info("~Rsm");
    raftCh->close();
    if (consumerThread.joinable()) {
        consumerThread.join();
    }
    spdlog::info("Destroyed Rsm");
}


} // namespace raft
