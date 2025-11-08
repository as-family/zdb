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
#include "client/Config.hpp"
#include <vector>
#include <string>
#include "common/RetryPolicy.hpp"
#include <utility>
#include <stdexcept>
#include "common/Error.hpp"
#include <tuple>
#include <expected>
#include <mutex>
#include <unordered_map>
#include "proto/raft.grpc.pb.h"

namespace zdb {

std::unordered_map<std::string, KVRPCService::function_t>& getDefaultKVFunctions() {
    static std::unordered_map<std::string, KVRPCService::function_t> map {
        { "get", [](std::shared_ptr<zdb::kvStore::KVStoreService::Stub> stub,
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
        { "set", [](std::shared_ptr<zdb::kvStore::KVStoreService::Stub> stub,
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
        { "erase", [](std::shared_ptr<zdb::kvStore::KVStoreService::Stub> stub,
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
        { "size", [](std::shared_ptr<zdb::kvStore::KVStoreService::Stub> stub,
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
    return map;
}

std::unordered_map<std::string, typename zdb::RPCService<raft::proto::Raft>::function_t>& getDefaultRaftFunctions() {
    static std::unordered_map<std::string, typename zdb::RPCService<raft::proto::Raft>::function_t> map {
        { "appendEntries", [](std::shared_ptr<raft::proto::Raft::Stub> stub,
                                grpc::ClientContext* ctx,
                                const google::protobuf::Message& req,
                                google::protobuf::Message* resp) -> grpc::Status {
            if (!resp ||
                req.GetDescriptor() != raft::proto::AppendEntriesArg::descriptor()) {
                return {grpc::StatusCode::INVALID_ARGUMENT,
                        "appendEntries: type mismatch or null resp"};
            }
            auto& r = static_cast<const raft::proto::AppendEntriesArg&>(req);
            auto* p = static_cast<raft::proto::AppendEntriesReply*>(resp);
            return stub->appendEntries(ctx, r, p);
        }},
        { "requestVote", [](std::shared_ptr<raft::proto::Raft::Stub> stub,
                            grpc::ClientContext* ctx,
                            const google::protobuf::Message& req,
                            google::protobuf::Message* resp) -> grpc::Status {
            if (!resp ||
                req.GetDescriptor() != raft::proto::RequestVoteArg::descriptor()) {
                return {grpc::StatusCode::INVALID_ARGUMENT,
                        "requestVote: type mismatch or null resp"};
            }
            auto& r = static_cast<const raft::proto::RequestVoteArg&>(req);
            auto* p = static_cast<raft::proto::RequestVoteReply*>(resp);
            return stub->requestVote(ctx, r, p);
        }}
    };
    return map;
}

Config::iterator Config::nextActiveServiceIterator() {
    for (auto i = services.begin(); i != services.end(); ++i) {
        if (i == cService) {
            continue;
        }
        if (i->second.available()) {
            return i;
        }
    }
    return services.end();
}

Config::Config(const std::vector<std::string>& addresses, const RetryPolicy p, std::unordered_map<std::string, KVRPCService::function_t> f)
    : policy{p},
      rng{std::random_device{}()} {
    if (addresses.empty()) {
        throw std::invalid_argument("Config: No addresses provided");
    }
    dist = std::uniform_int_distribution<std::size_t>(0, addresses.size() - 1);
    for (auto address : addresses) {
        services.emplace(std::piecewise_construct, 
                        std::forward_as_tuple(address), 
                        std::forward_as_tuple(address, p, f, stopCalls));
    }
    cService = services.end();
}

std::expected<KVRPCServicePtr, Error> Config::nextService() {
    std::lock_guard lock{m};
    if (cService != services.end() && cService->second.available()) {
        return &(cService->second);
    }

    cService = nextActiveServiceIterator();
    if (cService == services.end()) {
        return std::unexpected {Error(ErrorCode::AllServicesUnavailable, "No available services left")};
    }
    return &(cService->second);
}


std::expected<KVRPCServicePtr, Error> Config::randomService() {
    std::lock_guard lock{m};
    for (size_t j = 0; j < 10 * services.size(); ++j) {
        auto i = std::next(services.begin(), static_cast<std::ptrdiff_t>(dist(rng)));
        if (i == cService) {
            continue;
        }
        if (i->second.available()) {
            cService = i;
            return &i->second;
        }
    }
    return std::unexpected {Error(ErrorCode::AllServicesUnavailable, "No available services left")};
}

} // namespace zdb
