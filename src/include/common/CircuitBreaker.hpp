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
#ifndef CIRCUIT_BREAKER_H
#define CIRCUIT_BREAKER_H

#include <functional>
#include "common/RetryPolicy.hpp"
#include "common/Repeater.hpp"
#include <grpcpp/support/status.h>
#include <vector>
#include <string>
#include <chrono>

namespace zdb {

class CircuitBreaker {
public:
    enum class State : char {
        Open,
        Closed,
        HalfOpen
    };
    CircuitBreaker(const RetryPolicy p, std::atomic<bool>& sc);
    std::vector<grpc::Status> call(const std::string& op, const std::function<grpc::Status()>& rpc);
    [[nodiscard]] bool open();
    void stop();
private:
    State state;
    RetryPolicy policy;
    Repeater repeater;
    std::chrono::steady_clock::time_point lastFailureTime;
    std::atomic<bool>& stopCalls;
};

} // namespace zdb

#endif // CIRCUIT_BREAKER_H
