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
#include "lock/Lock.hpp"
#include "common/Types.hpp"
#include <random>
#include <string>
#include <algorithm>
#include "common/Util.hpp"

namespace zdb {

Lock::Lock(const Key key, KVStoreClient& c) : lockKey(key), lockValue {"", 1000}, client(c) {}

void Lock::acquire() {
    auto c = zdb_generate_random_alphanumeric_string(16);
    client.waitSet(lockKey, Value{c, 0});
    lockValue = Value{c, 1};
}

void Lock::release() {
    auto v = client.waitGet(lockKey, 1);
    if (v != lockValue) {
        return;
    }
    client.waitSet(lockKey, v);
    while (true) {
        auto t = client.erase(lockKey);
        if (t.has_value()) {
            return;
        } else {
            if (client.waitNotFound(lockKey)) {
                return;
            } else {
                if (!client.waitGet(lockKey, Value{v.data, 2})) {
                    return;
                }
            }
        }
    }
    std::unreachable();
}

} // namespace zdb
