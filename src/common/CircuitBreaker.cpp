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
#include "common/CircuitBreaker.hpp"
#include "common/Error.hpp"
#include "common/ErrorConverter.hpp"
#include <spdlog/spdlog.h>
#include "common/RetryPolicy.hpp"
#include <grpcpp/support/status.h>
#include <functional>
#include <chrono>
#include <utility>
#include <vector>
#include <string>
#include <iostream>

namespace zdb {

CircuitBreaker::CircuitBreaker(const RetryPolicy p, std::atomic<bool>& sc)
    : state{State::Closed},
      policy{p},
      repeater{p, sc},
      stopCalls{sc} {}

std::vector<grpc::Status> CircuitBreaker::call(const std::string& op, const std::function<grpc::Status()>& rpc) {
    switch (state) {
        case State::Open:
            if (std::chrono::steady_clock::now() - lastFailureTime < policy.resetTimeout) {
                return {grpc::Status(grpc::StatusCode::UNAVAILABLE, "Circuit breaker is open")};
            }
            state = State::HalfOpen;
            [[fallthrough]];
        case State::HalfOpen:
        {
            auto status = rpc();
            if (status.ok()) {
                state = State::Closed;
                repeater.reset();
            } else {
                // Log failure observed in half-open
                std::cerr << "CircuitBreaker HalfOpen observed failure: " << status.error_code() << " " << status.error_message() << std::endl;
                if (isRetriable(op, toError(status).code)) {
                    lastFailureTime = std::chrono::steady_clock::now();
                    state = State::Open;
                    std::cerr << "CircuitBreaker transitioning to Open (HalfOpen) due to retriable error" << std::endl;
                } else {
                    state = State::Closed;
                }
            }
            return {status};
        }
        case State::Closed:
        {
            auto statuses = repeater.attempt(op, rpc);
            if (!statuses.back().ok()) {
                // Log the failing status
                auto s = statuses.back();
                std::cerr << "CircuitBreaker Closed observed failing status: " << s.error_code() << " " << s.error_message() << std::endl;
                if (isRetriable(op, toError(statuses.back()).code)) {
                    lastFailureTime = std::chrono::steady_clock::now();
                    state = State::Open;
                    std::cerr << "CircuitBreaker transitioning to Open (Closed) due to retriable error" << std::endl;
                }
            }
            return statuses;
        }
    }
    std::unreachable();
}

bool CircuitBreaker::open() {
    if (state == State::Open && std::chrono::steady_clock::now() - lastFailureTime >= policy.resetTimeout) {
        state = State::HalfOpen;
    }
    return state == CircuitBreaker::State::Open;
}

void CircuitBreaker::stop() {
    repeater.stop();
}

} // namespace zdb
