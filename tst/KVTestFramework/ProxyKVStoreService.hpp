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
#ifndef PROXY_KV_STORE_SERVICE_H
#define PROXY_KV_STORE_SERVICE_H

#include "proto/kvStore.grpc.pb.h"
#include "KVTestFramework/ProxyService.hpp"

class ProxyKVStoreService : public zdb::kvStore::KVStoreService::Service {
public:
    ProxyKVStoreService(ProxyService<zdb::kvStore::KVStoreService>& p);
    grpc::Status get(
        grpc::ServerContext* context,
        const zdb::kvStore::GetRequest* request,
        zdb::kvStore::GetReply* reply) override;
    grpc::Status set(
        grpc::ServerContext* context,
        const zdb::kvStore::SetRequest* request,
        zdb::kvStore::SetReply* reply) override;
    grpc::Status erase(
        grpc::ServerContext* context,
        const zdb::kvStore::EraseRequest* request,
        zdb::kvStore::EraseReply* reply) override;
    grpc::Status size(
        grpc::ServerContext* context,
        const zdb::kvStore::SizeRequest* request,
        zdb::kvStore::SizeReply* reply) override;
private:
    ProxyService<zdb::kvStore::KVStoreService>& proxy;
};

#endif // PROXY_KV_STORE_SERVICE_H
