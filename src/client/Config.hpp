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
#ifndef CONFIG_H
#define CONFIG_H

#include <unordered_map>
#include <expected>
#include "common/Error.hpp"
#include "common/RPCService.hpp"
#include <proto/kvStore.grpc.pb.h>
#include <proto/kvStore.pb.h>
#include <random>
#include <vector>
#include "common/RetryPolicy.hpp"
#include <mutex>
#include <string>
#include <functional>

namespace zdb {

using KVRPCService = RPCService<zdb::kvStore::KVStoreService>;
using KVRPCServicePtr = RPCService<zdb::kvStore::KVStoreService>*;

inline std::unordered_map<std::string, KVRPCService::function_t> getDefaultKVFunctions() {
    return {
        { "get", [](zdb::kvStore::KVStoreService::Stub* stub,
                    grpc::ClientContext* ctx,
                    const google::protobuf::Message& req,
                    google::protobuf::Message* resp) -> grpc::Status {
            if (!resp ||
                req.GetDescriptor() != zdb::kvStore::GetRequest::descriptor()) {
                return {grpc::StatusCode::INVALID_ARGUMENT,
                        "get: type mismatch or null resp"};
            }
            auto& r = static_cast<const zdb::kvStore::GetRequest&>(req);
            auto* p = static_cast<zdb::kvStore::GetReply*>(resp);
            return stub->get(ctx, r, p);
        }},
        { "set", [](zdb::kvStore::KVStoreService::Stub* stub,
                    grpc::ClientContext* ctx,
                    const google::protobuf::Message& req,
                    google::protobuf::Message* resp) -> grpc::Status {
            if (!resp ||
                req.GetDescriptor() != zdb::kvStore::SetRequest::descriptor()) {
                return {grpc::StatusCode::INVALID_ARGUMENT,
                        "set: type mismatch or null resp"};
            }
            auto& r = static_cast<const zdb::kvStore::SetRequest&>(req);
            auto* p = static_cast<zdb::kvStore::SetReply*>(resp);
            return stub->set(ctx, r, p);
        }},
        { "erase", [](zdb::kvStore::KVStoreService::Stub* stub,
                      grpc::ClientContext* ctx,
                      const google::protobuf::Message& req,
                      google::protobuf::Message* resp) -> grpc::Status {
            if (!resp ||
                req.GetDescriptor() != zdb::kvStore::EraseRequest::descriptor()) {
                return {grpc::StatusCode::INVALID_ARGUMENT,
                        "erase: type mismatch or null resp"};
            }
            auto& r = static_cast<const zdb::kvStore::EraseRequest&>(req);
            auto* p = static_cast<zdb::kvStore::EraseReply*>(resp);
            return stub->erase(ctx, r, p);
        }},
        { "size", [](zdb::kvStore::KVStoreService::Stub* stub,
                     grpc::ClientContext* ctx,
                     const google::protobuf::Message& req,
                     google::protobuf::Message* resp) -> grpc::Status {
            if (!resp ||
                req.GetDescriptor() != zdb::kvStore::SizeRequest::descriptor()) {
                return {grpc::StatusCode::INVALID_ARGUMENT,
                        "size: type mismatch or null resp"};
            }
            auto& r = static_cast<const zdb::kvStore::SizeRequest&>(req);
            auto* p = static_cast<zdb::kvStore::SizeReply*>(resp);
            return stub->size(ctx, r, p);
        }}
    };
}

class Config {
public:
    using map = std::unordered_map<std::string, KVRPCService>;
    using iterator = map::iterator;
    Config(const std::vector<std::string>& addresses, const RetryPolicy policy, std::unordered_map<std::string, KVRPCService::function_t> f = getDefaultKVFunctions());
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    std::expected<KVRPCServicePtr, Error> nextService();
    std::expected<KVRPCServicePtr, Error> randomService();
    void resetUsed();
    const RetryPolicy policy;
private:
    std::mutex m;
    std::atomic<bool> stopCalls {false};
    iterator nextActiveServiceIterator();
    map services;
    iterator cService;
    std::default_random_engine rng;
    std::uniform_int_distribution<std::size_t> dist;

};
} // namespace zdb

#endif // CONFIG_H
