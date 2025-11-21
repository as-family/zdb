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
#include "server/KVStoreServiceImpl.hpp"
#include "common/ErrorConverter.hpp"
#include "common/Error.hpp"
#include <grpcpp/support/status.h>
#include "proto/kvStore.pb.h"
#include "raft/Command.hpp"
#include "raft/Raft.hpp"
#include "common/Command.hpp"
#include "common/Types.hpp"
#include <variant>
#include <tuple>
#include <sys/stat.h>

#include "common/Util.hpp"

namespace zdb {

KVStoreServiceImpl::KVStoreServiceImpl(raft::Rsm& r)
    : rsm {r} {}

grpc::Status KVStoreServiceImpl::get(
    grpc::ServerContext *context,
    const kvStore::GetRequest *request,
    kvStore::GetReply *reply) {
    const Key key{request->key().data()};
    auto uuid = string_to_uuid_v7(request->requestid().uuid());
    std::shared_ptr<raft::Command> g = std::make_shared<Get>(uuid, key);
    auto p = rsm.handle(g, std::min(context->deadline(), std::chrono::system_clock::now() + std::chrono::milliseconds{1000L}));
    const auto state = static_cast<State*>(p.get());
    if (std::holds_alternative<Error>(state->u)) {
        return toGrpcStatus(std::get<Error>(state->u));
    }
    if (const auto& v = std::get<std::optional<Value>>(state->u); !v.has_value()) {
        return toGrpcStatus(Error {ErrorCode::KeyNotFound, "key not found"});
    } else {
        reply->mutable_value()->set_data(v.value().data);
        reply->mutable_value()->set_version(v.value().version);
        return grpc::Status::OK;
    }
}

grpc::Status KVStoreServiceImpl::set(
    grpc::ServerContext *context,
    const kvStore::SetRequest* request,
    kvStore::SetReply *reply) {
    std::ignore = reply;
    const Key key{request->key().data()};
    const Value value{request->value().data(), request->value().version()};
    auto uuid = string_to_uuid_v7(request->requestid().uuid());
    std::shared_ptr<raft::Command> s = std::make_shared<Set>(uuid, key, value);
    auto p = rsm.handle(s, std::min(context->deadline(), std::chrono::system_clock::now() + std::chrono::milliseconds{1000L}));
    const auto state = static_cast<State*>(p.get());
    if (std::holds_alternative<Error>(state->u)) {
        return toGrpcStatus(std::get<Error>(state->u));
    }
    return grpc::Status::OK;
}

grpc::Status KVStoreServiceImpl::erase(
    grpc::ServerContext* context,
    const kvStore::EraseRequest* request,
    kvStore::EraseReply* reply) {
    const Key key{request->key().data()};
    auto uuid = string_to_uuid_v7(request->requestid().uuid());
    std::shared_ptr<raft::Command> e = std::make_shared<Erase>(uuid, key);
    auto p = rsm.handle(e, std::min(context->deadline(), std::chrono::system_clock::now() + std::chrono::milliseconds{1000L}));
    const auto state = static_cast<State*>(p.get());
    if (std::holds_alternative<Error>(state->u)) {
        return toGrpcStatus(std::get<Error>(state->u));
    }
    if (const auto& v = std::get<std::optional<Value>>(state->u); !v.has_value()) {
        return toGrpcStatus(Error {ErrorCode::KeyNotFound, "key not found"});
    } else {
        reply->mutable_value()->set_data(v.value().data);
        reply->mutable_value()->set_version(v.value().version);
        return grpc::Status::OK;
    }
}

grpc::Status KVStoreServiceImpl::size(
    grpc::ServerContext *context,
    const kvStore::SizeRequest *request,
    kvStore::SizeReply *reply) {
    auto uuid = string_to_uuid_v7(request->requestid().uuid());
    std::shared_ptr<raft::Command> sz = std::make_shared<Size>(uuid);
    auto p = rsm.handle(std::move(sz), std::min(context->deadline(), std::chrono::system_clock::now() + std::chrono::milliseconds{1000L}));
    const auto state = static_cast<State*>(p.get());
    if (std::holds_alternative<Error>(state->u)) {
        return toGrpcStatus(std::get<Error>(state->u));
    }
    auto v = std::get<size_t>(state->u);
    reply->set_size(v);
    return grpc::Status::OK;
}

} // namespace zdb
