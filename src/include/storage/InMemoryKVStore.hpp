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
#ifndef IN_MEMORY_KV_STORE_H
#define IN_MEMORY_KV_STORE_H

#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <mutex>
#include <cctype>
#include <expected>
#include <optional>
#include "common/Error.hpp"
#include "storage/StorageEngine.hpp"

namespace zdb {

class InMemoryKVStore : public StorageEngine {
public:
    InMemoryKVStore();
    std::expected<std::optional<Value>, Error> get(const Key& key) const override;
    std::expected<std::monostate, Error> set(const Key& key, const Value& value) override;
    std::expected<std::optional<Value>, Error> erase(const Key& key) override;
    size_t size() const;
private:
    std::unordered_map<Key, Value, KeyHash> store;
    mutable std::shared_mutex m;
};

} // namespace zdb

#endif // IN_MEMORY_KV_STORE_H
