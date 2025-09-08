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
#ifndef LOCKED_UNORDERED_MAP_H
#define LOCKED_UNORDERED_MAP_H

#include <unordered_map>
#include <shared_mutex>
#include <mutex>
#include <utility>

namespace zdb {

template <typename Key, typename Value>
class LockedUnorderedMap {
public:
    // Emplace function supporting piecewise construction
    template<typename... Args>
    std::pair<typename std::unordered_map<Key, Value>::iterator, bool> 
    emplace(Args&&... args) {
        std::unique_lock lock{mutex};
        return map.emplace(std::forward<Args>(args)...);
    }

    // Access element with read lock
    Value& at(const Key& key) {
        std::unique_lock lock{mutex};
        return map.at(key);
    }

    const Value& at(const Key& key) const {
        std::shared_lock lock{mutex};
        return map.at(key);
    }

    // Find element with read lock
    typename std::unordered_map<Key, Value>::iterator find(const Key& key) {
        std::unique_lock lock{mutex};
        return map.find(key);
    }

    typename std::unordered_map<Key, Value>::const_iterator find(const Key& key) const {
        std::shared_lock lock{mutex};
        return map.find(key);
    }

    // Size with read lock
    size_t size() const {
        std::shared_lock lock{mutex};
        return map.size();
    }

    // Begin/end iterators (note: these require external synchronization for safe iteration)
    auto begin() { 
        std::shared_lock lock{mutex};
        return map.begin(); 
    }
    
    auto end() { 
        std::shared_lock lock{mutex};
        return map.end(); 
    }

    auto begin() const { 
        std::shared_lock lock{mutex};
        return map.begin(); 
    }
    
    auto end() const { 
        std::shared_lock lock{mutex};
        return map.end(); 
    }

    // Subscript operator - returns reference to value, creates if doesn't exist
    Value& operator[](const Key& key) {
        std::unique_lock lock{mutex};
        return map[key];
    }

    Value& operator[](Key&& key) {
        std::unique_lock lock{mutex};
        return map[std::move(key)];
    }

    // WARNING: This function returns a reference to the underlying map without locking.
    // It is NOT thread-safe and should only be used when external synchronization is guaranteed.
    // The caller is responsible for ensuring no concurrent access occurs.
    std::unordered_map<Key, Value>& stdMap() {
        return map;
    }

    const std::unordered_map<Key, Value>& stdMap() const {
        return map;
    }

private:
    std::unordered_map<Key, Value> map;
    mutable std::shared_mutex mutex;
};

} // namespace zdb

#endif // LOCKED_UNORDERED_MAP_H
