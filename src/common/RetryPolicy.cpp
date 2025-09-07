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
#include "RetryPolicy.hpp"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <chrono>

namespace zdb {

RetryPolicy::RetryPolicy(
    std::chrono::microseconds base,
    std::chrono::microseconds max,
    std::chrono::microseconds reset,
    int threshold,
    int services,
    std::chrono::milliseconds rpc,
    std::chrono::milliseconds channel)
    : baseDelay(base),
      maxDelay(max),
      resetTimeout(reset),
      failureThreshold(threshold),
      servicesToTry(services),
      rpcTimeout(rpc),
      channelTimeout(channel) {
    if (threshold < 0) {
        throw std::invalid_argument("Failure threshold must be >= zero.");
    }
    if (services < 0) {
        throw std::invalid_argument("Services to try must be >= zero.");
    }
    if (base < std::chrono::microseconds::zero()) {
        throw std::invalid_argument("Base delay must be >= zero.");
    }
    if (max < std::chrono::microseconds::zero()) {
        throw std::invalid_argument("Max delay must be >= zero.");
    }
    if (reset < std::chrono::microseconds::zero()) {
        throw std::invalid_argument("Reset timeout must be >= zero.");
    }
    if (rpc < std::chrono::milliseconds::zero()) {
        throw std::invalid_argument("RPC timeout must be >= zero.");
    }
    if (channel < std::chrono::milliseconds::zero()) {
        throw std::invalid_argument("Channel timeout must be >= zero.");
    }
    if (max < base) {
        throw std::invalid_argument("Max delay must be >= base delay.");
    }
}

} // namespace zdb
