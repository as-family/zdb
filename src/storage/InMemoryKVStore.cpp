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
#include "storage/InMemoryKVStore.hpp"
#include <expected>
#include <string>
#include <shared_mutex>
#include <mutex>
#include <cstddef>
#include "common/Error.hpp"
#include "common/Types.hpp"
#include <expected>
#include <optional>
#include <variant>

namespace zdb {

InMemoryKVStore::InMemoryKVStore() : store{}, m{} {}

std::expected<std::optional<Value>, Error> InMemoryKVStore::get(const Key& key) const {
    const std::shared_lock lock {m};
    auto i = store.find(key);
    if (i == store.end()) {
        return std::nullopt;
    }
    return i->second;
}

std::expected<std::monostate, Error> InMemoryKVStore::set(const Key& key, const Value& value) {
    const std::unique_lock lock {m};
    auto i = store.find(key);
    if (i != store.end()) {
        if (value.version != i->second.version) {
            return std::unexpected {Error {ErrorCode::VersionMismatch, "Version mismatch: expected " + std::to_string(i->second.version) + " but got " + std::to_string(value.version), key.data, i->second.data, i->second.version}};
        }
        i->second = Value{value.data, i->second.version + 1};
    } else {
        if (value.version != 0) {
            return std::unexpected {Error {ErrorCode::KeyNotFound, "Key does not exist, must use version 0 for new keys", key.data, value.data, 0}};
        }
        store.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(value.data, 1));
    }
    return {};
}

std::expected<std::optional<Value>, Error> InMemoryKVStore::erase(const Key& key) {
    const std::unique_lock lock {m};
    auto i = store.find(key);
    if (i == store.end()) {
        return std::nullopt;
    }
    auto v = i->second;
    store.erase(i);
    return v;
}

size_t InMemoryKVStore::size() const {
    const std::shared_lock lock {m};
    return store.size();
}

} // namespace zdb
