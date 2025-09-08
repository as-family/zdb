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
#ifndef STORAGE_ENGINE_HPP
#define STORAGE_ENGINE_HPP
#include "common/Types.hpp"
#include "common/Error.hpp"
#include <expected>
#include <optional>
#include <variant>

namespace zdb {

class StorageEngine {
public:
    virtual ~StorageEngine() = default;

    virtual std::expected<std::optional<Value>, Error> get(const Key& key) const = 0;
    virtual std::expected<std::monostate, Error> set(const Key& key, const Value& value) = 0;
    virtual std::expected<std::optional<Value>, Error> erase(const Key& key) = 0;
    virtual size_t size() const = 0;
};

} // namespace zdb

#endif // STORAGE_ENGINE_HPP
