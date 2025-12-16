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
#ifndef RAFT_CHANNEL_H
#define RAFT_CHANNEL_H

#include <optional>
#include <string>
#include <chrono>
#include <expected>

namespace raft {

enum class ChannelError {
    Closed
};

template <typename T>
class Channel {
public:
    virtual ~Channel() = default;
    virtual void send(T) = 0;
    virtual bool sendUntil(T, std::chrono::system_clock::time_point t) = 0;
    virtual std::optional<T> receive() = 0;
    virtual std::expected<std::optional<T>, ChannelError> receiveUntil(std::chrono::system_clock::time_point t) = 0;
    virtual void close() = 0;
};

} // namespace raft

#endif // RAFT_CHANNEL_H
