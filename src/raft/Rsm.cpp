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

namespace raft {

Rsm::Rsm(std::shared_ptr<StateMachine> m, std::shared_ptr<raft::Channel<std::shared_ptr<raft::Command>>> rCh, std::shared_ptr<raft::Raft> r)
    : raftCh {rCh}, raft {r}, machine {m}, consumerThread {&raft::Rsm::consumeChannel, this} {}

void Rsm::consumeChannel() {
    while (!raftCh->isClosed()) {
        std::unique_lock lock{m};
        auto c = raftCh->receiveUntil(std::chrono::system_clock::now() + std::chrono::milliseconds{1L});
        if (c.has_value()) {
            machine->applyCommand(*c.value());
        }
    }
}

std::unique_ptr<raft::State> Rsm::handle(std::shared_ptr<raft::Command> c, std::chrono::system_clock::time_point t) {
    spdlog::info("Rsm::handle");
    std::unique_lock lock{m};
    if (!raft->start(c)) {
        spdlog::info("Rsm::handle: not leader");
        return std::make_unique<zdb::State>(zdb::State{zdb::Error{zdb::ErrorCode::NotLeader, "not the leader"}});
    }
    while (!raftCh->isClosed()) {
        spdlog::info("Rsm::handle: channel open");
        auto r = raftCh->receiveUntil(t);
        if (!r.has_value()) {
            spdlog::info("Rsm::handle: timedout");
            return std::make_unique<zdb::State>(zdb::State{zdb::Error{zdb::ErrorCode::Timeout, "request timed out"}});
        }
        spdlog::info("Rsm::handle calling machine");
        auto s = machine->applyCommand(*r.value());
        if (r.value()->getUUID() == c->getUUID()) {
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
