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
#include "client/KVStoreClient.hpp"
#include <spdlog/spdlog.h>
#include "client/Config.hpp"
#include <string>
#include "proto/kvStore.pb.h"
#include "proto/kvStore.grpc.pb.h"
#include "common/Error.hpp"
#include "common/Types.hpp"
#include <cstddef>
#include <expected>

namespace zdb {

KVStoreClient::KVStoreClient(Config& c) : config(c) {}

std::expected<Value, Error> KVStoreClient::get(const Key& key) const {
    kvStore::GetRequest request;
    request.mutable_key()->set_data(key.data);
    auto t = call<kvStore::GetRequest, kvStore::GetReply>(
        "get",
        request
    );
    if (t.has_value()) {
        return t.value().value();
    } else {
        return std::unexpected {t.error().back()};
    }
}

std::expected<std::monostate, Error> KVStoreClient::set(const Key& key, const Value& value) {
    kvStore::SetRequest request;
    request.mutable_key()->set_data(key.data);
    request.mutable_value()->set_data(value.data);
    request.mutable_value()->set_version(value.version);
    auto t = call<kvStore::SetRequest, kvStore::SetReply>(
        "set",
        request
    );

    if (t.has_value()) {
        return {};
    } else {
        if (t.error().size() > 1 && isRetriable("set", t.error().front().code) && t.error().back().code == ErrorCode::VersionMismatch) {
            return std::unexpected {Error(ErrorCode::Maybe)};
        } else {
            return std::unexpected {t.error().back()};
        }
    }
}

std::expected<Value, Error> KVStoreClient::erase(const Key& key) {
    kvStore::EraseRequest request;
    request.mutable_key()->set_data(key.data);
    auto t = call<kvStore::EraseRequest, kvStore::EraseReply>(
        "erase",
        request
    );
    if (t.has_value()) {
        return t.value().value();
    } else {
        return std::unexpected {t.error().back()};
    }
}

std::expected<size_t, Error> KVStoreClient::size() const {
    kvStore::SizeRequest request;
    auto t = call<kvStore::SizeRequest, kvStore::SizeReply>(
        "size",
        request
    );
    if (t.has_value()) {
        return t.value().size();
    } else {
        return std::unexpected {t.error().back()};
    }
}


void KVStoreClient::waitSet(Key key, Value value) {
    while(true) {
        auto v = set(key, value);
        if (v.has_value()) {
            return;
        } else {
            if (waitGet(key, Value{value.data, value.version + 1})) {
                return;
            }
        }
    }
    std::unreachable();
}

bool KVStoreClient::waitGet(Key key, Value value) {
    while (true) {
        auto t = get(key);
        if(t.has_value()) {
            if(t.value() == value) {
                return true;
            } else {
                return false;
            }
        } else {
            if (t.error().code == ErrorCode::KeyNotFound) {
                return false;
            }
        }
    }
    std::unreachable();
}

bool KVStoreClient::waitNotFound(Key key) {
    while (true) {
        auto t = get(key);
        if (t.has_value()) {
            return false;
        } else {
            if (t.error().code == ErrorCode::KeyNotFound) {
                return true;
            }
        }
    }
    std::unreachable();
}

Value KVStoreClient::waitGet(Key key, uint64_t version) {
    while (true) {
        auto t = get(key);
        if (t.has_value()) {
            if (t.value().version == version) {
                return t.value();
            }
        }
    }
    std::unreachable();
}

} // namespace zdb
