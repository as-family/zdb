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
#ifndef TYPES_HPP
#define TYPES_HPP

#include <string>
#include "proto/types.pb.h"
#include "raft/StateMachine.hpp"
#include <expected>
#include <optional>
#include "common/Error.hpp"
#include <variant>
#include <functional>

namespace zdb {

struct Key {
    std::string data;
    
    Key(const std::string& d) : data(d) {}
    
    Key(const proto::Key& protoKey);

    bool operator==(const Key& other) const {
        return data == other.data;
    }
};

struct KeyHash {
    std::size_t operator()(const Key& key) const {
        return std::hash<std::string>()(key.data);
    }
};

struct Value {
    std::string data;
    uint64_t version = 0;
    
    Value(const std::string& d, uint64_t v = 0) : data(d), version(v) {}
    
    Value(const proto::Value& protoValue);

    bool operator==(const Value& other) const {
        return data == other.data && version == other.version;
    }

    Value(const Value&) = default;
    Value& operator=(const Value&) = default;  
    Value(Value&&) = default;  
    Value& operator=(Value&&) = default; 
};

struct State : public raft::State {
    std::optional<Key> key;
    std::variant<
        Error,
        std::optional<Value>,
        std::monostate,
        size_t
    > u;
    State(const Key& k, const std::expected<std::optional<Value>, Error>& v)
        : key{k}, u {std::nullopt} {
        if (v.has_value()) {
            u = v.value();
        } else {
            u = v.error();
        }
    }
    State(const Key& k, const std::expected<std::monostate, Error>& v)
        : key{k}, u{std::monostate {}} {
        if (v.has_value()) {
            u = v.value();
        } else {
            u = v.error();
        }
    }
    State(const Key& k, const std::expected<size_t, Error>& v)
        : key{k}, u{static_cast<size_t>(0)} {
        if (v.has_value()) {
            u = v.value();
        } else {
            u = v.error();
        }
    }
    State(const Key& k, const std::optional<Value>& v)
        : key(k), u{v} {}
    State(const Key& k, const std::monostate v)
        : key(k), u{v} {}
    explicit State(const size_t v)
        : key(std::nullopt), u{v} {}
    explicit  State(const Error& error)
        : key{std::nullopt}, u{error} {}
};

} // namespace zdb

#endif // TYPES_HPP
