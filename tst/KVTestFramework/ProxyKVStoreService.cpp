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

#include "ProxyKVStoreService.hpp"
#include <grpcpp/grpcpp.h>
#include "common/ErrorConverter.hpp"

ProxyKVStoreService::ProxyKVStoreService(ProxyService<zdb::kvStore::KVStoreService>& p)
    : proxy{p} {}

grpc::Status ProxyKVStoreService::get(
    grpc::ServerContext* context,
    const zdb::kvStore::GetRequest* request,
    zdb::kvStore::GetReply* reply) {
    auto t = proxy.call<zdb::kvStore::GetRequest, zdb::kvStore::GetReply>("get", *request, context->deadline());
    if (t.has_value()) {
        *reply = t.value();
        return grpc::Status::OK;
    }
    return zdb::toGrpcStatus(t.error().back());
}

grpc::Status ProxyKVStoreService::set(
    grpc::ServerContext* context,
    const zdb::kvStore::SetRequest* request,
    zdb::kvStore::SetReply* reply) {
    auto t = proxy.call<zdb::kvStore::SetRequest, zdb::kvStore::SetReply>("set", *request, context->deadline());
    if (t.has_value()) {
        *reply = t.value();
        return grpc::Status::OK;
    }
    return zdb::toGrpcStatus(t.error().back());
}

grpc::Status ProxyKVStoreService::erase(
    grpc::ServerContext* context,
    const zdb::kvStore::EraseRequest* request,
    zdb::kvStore::EraseReply* reply) {
    auto t = proxy.call<zdb::kvStore::EraseRequest, zdb::kvStore::EraseReply>("erase", *request, context->deadline());
    if (t.has_value()) {
        *reply = t.value();
        return grpc::Status::OK;
    }
    return zdb::toGrpcStatus(t.error().back());
}

grpc::Status ProxyKVStoreService::size(
    grpc::ServerContext* context,
    const zdb::kvStore::SizeRequest* request,
    zdb::kvStore::SizeReply* reply) {
    auto t = proxy.call<zdb::kvStore::SizeRequest, zdb::kvStore::SizeReply>("size", *request, context->deadline());
    if (t.has_value()) {
        *reply = t.value();
        return grpc::Status::OK;
    }
    return zdb::toGrpcStatus(t.error().back());
}
