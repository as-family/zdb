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
#ifndef PROXYSERVICE_HPP
#define PROXYSERVICE_HPP

#include <memory>
#include <grpcpp/grpcpp.h>
#include "KVTestFramework/NetworkConfig.hpp"
#include <expected>
#include <vector>
#include "common/Error.hpp"
#include <thread>
#include <string>
#include "common/ErrorConverter.hpp"
#include <mutex>
#include <condition_variable>
#include "common/RetryPolicy.hpp"
#include "common/RPCService.hpp"
#include <proto/kvStore.grpc.pb.h>
#include <proto/kvStore.pb.h>
#include <proto/raft.grpc.pb.h>
#include <proto/raft.pb.h>
#include <unordered_map>
#include <functional>
#include <chrono>
#include "raft/Types.hpp"
#include <type_traits>
#include "common/TypesMap.hpp"

inline std::unordered_map<std::string, typename zdb::RPCService<zdb::kvStore::KVStoreService>::function_t> getDefaultKVProxyFunctions() {
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

inline std::unordered_map<std::string, typename zdb::RPCService<raft::proto::Raft>::function_t> getDefaultRaftProxyFunctions() {
    return {
        { "appendEntries", [](raft::proto::Raft::Stub* stub,
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
        { "requestVote", [](raft::proto::Raft::Stub* stub,
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
}

template<typename Service>
class ProxyService {
    using Stub = typename Service::Stub;
public:
    ProxyService(const std::string& original, NetworkConfig& c, zdb::RetryPolicy p)
    : originalAddress {original},
      networkConfig {c},
      policy {p} {}

    ProxyService(const std::string& original, NetworkConfig& c, zdb::RetryPolicy p, std::unordered_map<std::string, typename zdb::RPCService<Service>::function_t> f)
    : originalAddress {original},
      functions {std::move(f)},
      networkConfig {c},
      policy {p} {}

    ProxyService(const std::string& original, NetworkConfig& c, zdb::RetryPolicy p, std::unordered_map<std::string, typename zdb::RPCService<Service>::function_t> f, std::atomic<bool>& sc)
        : originalAddress {original},
          functions {std::move(f)},
          networkConfig {c},
          policy {p} {}

    template<typename Req, typename Rep = zdb::map_to_t<Req>>
    std::expected<Rep, std::vector<zdb::Error>> call(
        std::string op,
        const Req& request) {
        std::lock_guard l{m};
        if (!networkConfig.isConnected()) {
            return std::unexpected(std::vector<zdb::Error> {zdb::toError(grpc::Status(grpc::StatusCode::UNAVAILABLE, "Disconnected"))});
        }
        if (!stub || !channel) {
            return std::unexpected(std::vector<zdb::Error> {zdb::toError(grpc::Status(grpc::StatusCode::UNAVAILABLE, "Not Connected"))});
        }
        auto funcIt = functions.find(op);
        if (funcIt == functions.end()) {
            return std::unexpected(std::vector<zdb::Error> {zdb::toError(grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Unknown operation"))});
        }
        grpc::ClientContext c;
        c.set_deadline(std::chrono::system_clock::now() + policy.rpcTimeout);
        auto& reqMsg = static_cast<const google::protobuf::Message&>(request);
        auto reply = Rep{};
        auto& repMsg = static_cast<google::protobuf::Message&>(reply);
        if (networkConfig.isReliable()) {
            std::cerr << "Reliable" << std::endl;
            auto status = funcIt->second(stub.get(), &c, reqMsg, &repMsg);
            if (status.ok()) {
                std::cerr << "OK" << std::endl;
                if constexpr (std::is_base_of_v<raft::Reply, Rep>) {
                    return Rep{repMsg};
                } else {
                    return reply;
                }
            } else {
                return std::unexpected(std::vector<zdb::Error> {zdb::toError(status)});
            }
        } else {
            if (networkConfig.shouldDrop()) {
                return std::unexpected(std::vector<zdb::Error> {zdb::toError(grpc::Status(grpc::StatusCode::DEADLINE_EXCEEDED, "Dropped"))});
            }
            auto status = funcIt->second(stub.get(), &c, reqMsg, &repMsg);
            if (networkConfig.shouldDrop()) {
               return std::unexpected(std::vector<zdb::Error> {zdb::toError(grpc::Status(grpc::StatusCode::DEADLINE_EXCEEDED, "Dropped"))});
            }
            if (networkConfig.shouldDelay()) {
                std::this_thread::sleep_for(networkConfig.delayTime());
            }
            if (status.ok()) {
                if constexpr (std::is_base_of_v<raft::Reply, Rep>) {
                    return Rep{repMsg};
                } else {
                    return reply;
                }
            } else {
                return std::unexpected(std::vector<zdb::Error> {zdb::toError(status)});
            }
        }
    }
    NetworkConfig& getNetworkConfig() {
        return networkConfig;
    }
    void connectTarget() {
        std::lock_guard l{m};
        channel = grpc::CreateChannel(originalAddress, grpc::InsecureChannelCredentials());
        if (!channel->WaitForConnected(std::chrono::system_clock::now() + std::chrono::milliseconds{500L})) {
            throw std::runtime_error("Failed to connect to channel");
        }
        stub = Service::NewStub(channel);
        if(!channel || !stub || channel->GetState(false) != grpc_connectivity_state::GRPC_CHANNEL_READY) {
            throw std::runtime_error("Failed to create channel or stub");
        }
    }
    std::string address() const {
        return originalAddress;
    }
private:
    std::mutex m{};
    std::string originalAddress;
    std::unordered_map<std::string, typename zdb::RPCService<Service>::function_t> functions;
    std::shared_ptr<grpc::Channel> channel;
    std::unique_ptr<Stub> stub;
    NetworkConfig& networkConfig;
    zdb::RetryPolicy policy;
};

#endif // PROXYSERVICE_HPP
