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
#ifndef LOCK_H
#define LOCK_H

#include "client/KVStoreClient.hpp"
#include <string>
#include "common/Types.hpp"

namespace zdb {

class Lock {
public:
    Lock(const Key key, KVStoreClient& c);
    void acquire();
    void release();
private:
    const Key lockKey;
    Value lockValue;
    KVStoreClient& client;
};

} // namespace zdb

#endif // LOCK_H
