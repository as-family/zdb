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
#ifndef RETRY_POLICY_H
#define RETRY_POLICY_H

#include <chrono>

namespace zdb {

struct RetryPolicy {
    RetryPolicy(
        std::chrono::microseconds base,
        std::chrono::microseconds max,
        std::chrono::microseconds reset,
        int threshold,
        int services,
        std::chrono::milliseconds rpc,
        std::chrono::milliseconds channel
    );
    std::chrono::microseconds baseDelay;
    std::chrono::microseconds maxDelay;
    std::chrono::microseconds resetTimeout;
    int failureThreshold;
    int servicesToTry;
    std::chrono::milliseconds rpcTimeout;
    std::chrono::milliseconds channelTimeout;
};

} // namespace zdb

#endif // RETRY_POLICY_H
