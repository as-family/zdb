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
#ifndef RAFT_SYNC_CHANNEL_H
#define RAFT_SYNC_CHANNEL_H

#include "raft/Command.hpp"
#include <mutex>
#include <optional>
#include <condition_variable>
#include "raft/Channel.hpp"
#include <cstddef>
#include <optional>

namespace raft {

class SyncChannel : public Channel {
public:
    void send(std::string) override;
    bool sendUntil(std::string, std::chrono::system_clock::time_point t) override;
    std::string receive() override;
    std::optional<std::string> receiveUntil(std::chrono::system_clock::time_point t) override;
    virtual void close() override;
    virtual bool isClosed() override;
    ~SyncChannel() override;
private:
    void doClose() noexcept;
    std::mutex m;
    std::condition_variable cv;
    std::optional<std::string> value;
    bool closed = false;
};

} // namespace raft

#endif // RAFT_SYNC_CHANNEL_H
