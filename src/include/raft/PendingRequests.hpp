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

#ifndef RAFT_PENDING_REQUESTS_H
#define RAFT_PENDING_REQUESTS_H

#include <unordered_map>
#include <mutex>
#include <future>
#include "raft/StateMachine.hpp"
#include "common/Util.hpp"

namespace raft {

class PendingRequests {
public:
    std::future<std::unique_ptr<State>> add(const std::string index);
    void complete(const std::string index, std::unique_ptr<State> result);
    void cancellAll();

private:
    std::mutex m;
    std::unordered_map<std::string, std::promise<std::unique_ptr<State>>> pending;
};

} // namespace raft

#endif // RAFT_PENDING_REQUESTS_H


