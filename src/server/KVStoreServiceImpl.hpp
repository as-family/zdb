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
#ifndef SRC_SERVER_KVSTORESERVICEIMPL_HPP
#define SRC_SERVER_KVSTORESERVICEIMPL_HPP

#include <memory>
#include <grpcpp/grpcpp.h>
#include <unordered_map>
#include "proto/kvStore.grpc.pb.h"
#include "server/RPCServer.hpp"
#include "common/KVStateMachine.hpp"

namespace zdb {

class KVStoreServiceImpl final : public kvStore::KVStoreService::Service {
public:
    KVStoreServiceImpl(KVStateMachine& kv);
    grpc::Status get(
        grpc::ServerContext* context,
        const kvStore::GetRequest* request,
        kvStore::GetReply* reply) override;
    grpc::Status set(
        grpc::ServerContext* context,
        const kvStore::SetRequest* request,
        kvStore::SetReply* reply) override;
    grpc::Status erase(
        grpc::ServerContext* context,
        const kvStore::EraseRequest* request,
        kvStore::EraseReply* reply) override;
    grpc::Status size(
        grpc::ServerContext* context,
        const kvStore::SizeRequest* request,
        kvStore::SizeReply* reply) override;
private:
    KVStateMachine& kvStateMachine;
};

using KVStoreServer = RPCServer<KVStoreServiceImpl>;

} // namespace zdb

#endif // SRC_SERVER_KVSTORESERVICEIMPL_HPP
