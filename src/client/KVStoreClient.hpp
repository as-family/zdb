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
#ifndef KV_STORE_CLIENT_H
#define KV_STORE_CLIENT_H

#include <expected>
#include <string>
#include <vector>
#include <spdlog/spdlog.h>
#include "client/Config.hpp"
#include "common/Error.hpp"
#include "common/RetryPolicy.hpp"
#include "common/Types.hpp"
#include "common/Util.hpp"
#include "common/TypesMap.hpp"

namespace zdb {

class KVStoreClient {
public:
    explicit KVStoreClient(Config& c);
    KVStoreClient(const KVStoreClient&) = delete;
    KVStoreClient& operator=(const KVStoreClient&) = delete;
    [[nodiscard]] std::expected<Value, Error> get(const Key& key) const;
    [[nodiscard]] std::expected<std::monostate, Error> set(const Key& key, const Value& value);
    [[nodiscard]] std::expected<Value, Error> erase(const Key& key);
    [[nodiscard]] std::expected<size_t, Error> size() const;
    void waitSet(Key key, Value value);
    bool waitGet(Key key, Value value);
    bool waitNotFound(Key key);
    Value waitGet(Key key, uint64_t version);
private:
    template<typename Req, typename Rep=map_to_t<Req>>
    std::expected<Rep, std::vector<Error>> call(
        const std::string& op,
        Req& request) const {
        request.mutable_requestid()->set_uuid(uuid_v7_to_string(generate_uuid_v7()));
        for (int i = 0; i < config.policy.servicesToTry; ++i) {
            auto serviceResult = config.nextService();
            if (serviceResult.has_value()) {
                std::cerr << "Calling " << serviceResult.value()->address() << std::endl;
                auto callResult = serviceResult.value()->call<Req, Rep>(op, request);
                if (callResult.has_value()) {
                    return callResult;
                } else if (callResult.error().back().code == ErrorCode::NotLeader) {
                    serviceResult = config.randomService();
                } else if (!isRetriable(op, callResult.error().back().code)) {
                    return callResult;
                }
            }
        }
        return std::unexpected {std::vector {Error(ErrorCode::AllServicesUnavailable, "All services are unavailable")}};
    }
    Config& config;
};

} // namespace zdb

#endif // KV_STORE_CLIENT_H
