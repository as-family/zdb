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
#include "GoRPCClient.hpp"
#include <iostream>
#include <memory>

GoRPCClient::GoRPCClient(int ii, std::string a, const zdb::RetryPolicy p, uintptr_t h)
    : i {ii}, address {a}, policy {p}, handle {h} {}

void GoRPCClient::stop() {
    decltype(breakers) local;
    {
        std::unique_lock lock{m};
        local = std::move(breakers);
        breakers.clear();
    }
    for (const auto& b : local) {
        b->stop();
    }
    // std::cerr << "C++: GoRPCClient to " << address << " is stopping\n";
}

GoRPCClient::~GoRPCClient() {
    stop();
}
