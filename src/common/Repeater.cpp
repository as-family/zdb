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
#include "Repeater.hpp"
#include "Error.hpp"
#include "ErrorConverter.hpp"
#include "RetryPolicy.hpp"
#include <functional>
#include <optional>
#include <chrono>
#include <thread>
#include <grpcpp/support/status.h>
#include <vector>
#include <string>

namespace zdb {

Repeater::Repeater(const RetryPolicy p)
    : backoff {p} {}

std::vector<grpc::Status> Repeater::attempt(const std::string& op, const std::function<grpc::Status()>& rpc) {
    std::vector<grpc::Status> statuses;
    while (!stopped.load(std::memory_order_acquire)) {
        auto status = rpc();
        if (stopped.load(std::memory_order_acquire)) {
            statuses.push_back(grpc::Status{grpc::StatusCode::CANCELLED, "Repeater stopped"});
            return statuses;
        }
        statuses.push_back(status);
        if (status.ok()) {
            backoff.reset();
            return statuses;
        } else {
            if (!isRetriable(op, toError(status).code)) {
                backoff.reset();
                return statuses;
            }
            auto delay = backoff.nextDelay()
                .and_then([this](std::chrono::microseconds v) {
                    return std::optional<std::chrono::microseconds> {fullJitter.jitter(v)};
                });

            if (delay.has_value()) {
                auto remaining = delay.value();
                while (!stopped.load(std::memory_order_acquire) && remaining > std::chrono::microseconds::zero()) {
                    auto step = std::min<std::chrono::microseconds>(remaining, std::chrono::microseconds{1000});
                    std::this_thread::sleep_for(step);
                    remaining -= step;
                }
                if (stopped.load(std::memory_order_acquire)) {
                    statuses.push_back(grpc::Status{grpc::StatusCode::CANCELLED, "Repeater stopped"});
                    return statuses;
                }
            } else {
                return statuses;
            }
        }
    }
    statuses.push_back(grpc::Status{grpc::StatusCode::CANCELLED, "Repeater stopped"});
    return statuses;
}

void Repeater::reset() {
    stopped.store(false, std::memory_order_release);
    backoff.reset();
}

void Repeater::stop() noexcept {
    stopped = true;
}

} // namespace zdb
