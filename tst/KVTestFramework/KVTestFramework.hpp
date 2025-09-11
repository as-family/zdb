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
#ifndef KV_TEST_FRAMEWORK_H
#define KV_TEST_FRAMEWORK_H

#include "common/Error.hpp"
#include "common/Types.hpp"
#include "ProxyKVStoreService.hpp"
#include <string>
#include "server/KVStoreServiceImpl.hpp"
#include "client/KVStoreClient.hpp"
#include "client/Config.hpp"
#include <vector>
#include <functional>
#include <atomic>
#include "common/Types.hpp"
#include <expected>
#include "Porcupine.hpp"
#include "raft/Raft.hpp"
#include "raft/Channel.hpp"
#include "raft/TestRaft.hpp"
#include "storage/InMemoryKVStore.hpp"
#include "common/KVStateMachine.hpp"
#include "common/RetryPolicy.hpp"
#include <random>
#include <variant>
#include "server/RPCServer.hpp"
#include "KVTestFramework/NetworkConfig.hpp"

class KVTestFramework {
public:
    struct ClientResult {
        int nOK;
        int nMaybe;
    };
    Porcupine porcupine;
    KVTestFramework(const std::string& a, const std::string& t, NetworkConfig& c, raft::Channel<std::unique_ptr<raft::Command>>& l, raft::Raft& r, zdb::RetryPolicy p);
    std::vector<ClientResult> spawnClientsAndWait(
        int nClients,
        std::chrono::seconds timeout,
        std::vector<std::string> addresses,
        zdb::RetryPolicy policy,
        std::function<ClientResult(int id, zdb::KVStoreClient& client, std::atomic<bool>& done)> f
    );
    ClientResult oneClientSet(
        int clientId,
        zdb::KVStoreClient& client,
        std::vector<zdb::Key> keys,
        bool randomKeys,
        std::atomic<bool>& done
    );
    std::pair<int, bool> oneSet(
        int clientId,
        zdb::KVStoreClient& client,
        zdb::Key key,
        uint64_t version
    );
    std::expected<std::monostate, zdb::Error> setJson(int clientId, zdb::KVStoreClient& client, zdb::Key key, zdb::Value value);
    zdb::Value getJson(int clientId, zdb::KVStoreClient& client, zdb::Key key);
    bool checkSetConcurrent(
        zdb::KVStoreClient& client,
        zdb::Key key,
        std::vector<ClientResult> results
    );
    ~KVTestFramework();
private:
    std::string addr;
    std::string targetServerAddr;
    NetworkConfig& networkConfig;
    zdb::InMemoryKVStore mem;
    raft::Channel<std::unique_ptr<raft::Command>>& leader;
    raft::Raft& raft;
    zdb::KVStateMachine kvState;
    zdb::KVStoreServiceImpl targetService;
    ProxyService<zdb::kvStore::KVStoreService> targetProxyService;
    ProxyKVStoreService service;
    zdb::RPCServer<zdb::KVStoreServiceImpl> targetServer;
    zdb::RPCServer<ProxyKVStoreService> server;
    std::default_random_engine rng;
};

#endif // KV_TEST_FRAMEWORK_H
