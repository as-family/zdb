// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * ZDB a distributed, fault-tolerant database.
 * Copyright (C) 2025 Ahmed Refaat Gadalla Mohamed
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "raft/PendingRequests.hpp"
#include "raft/StateMachine.hpp"
#include <mutex>
#include <future>
#include <spdlog/spdlog.h>
#include "common/Util.hpp"
#include "common/Types.hpp"

namespace raft {

std::future<std::unique_ptr<State>> PendingRequests::add(const std::string index) {
    std::lock_guard lock{m};
    std::promise<std::unique_ptr<State>> p;
    auto f = p.get_future();
    pending[index] = std::move(p);
    return f;
}

void PendingRequests::complete(const std::string index, std::unique_ptr<State> result) {
    std::lock_guard lock{m};
    auto it = pending.find(index);
    if (it != pending.end()) {
        it->second.set_value(std::move(result));
        pending.erase(it);
    } else {
        spdlog::debug("changed leader");
    }
}

void PendingRequests::cancelAll() {
    std::lock_guard lock{m};
    for (auto &p : pending) {
      p.second.set_value(std::make_unique<zdb::State>(zdb::Error{zdb::ErrorCode::NotLeader, "Changed Leader"}));
    }
    pending.clear();
}


} // namespace raft
