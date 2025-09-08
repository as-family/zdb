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
#include "KVTestFramework.hpp"
#include <string>
#include <grpcpp/grpcpp.h>
#include "storage/InMemoryKVStore.hpp"
#include "client/Config.hpp"
#include "client/KVStoreClient.hpp"
#include <vector>
#include <thread>
#include <unordered_map>
#include <expected>
#include "common/Types.hpp"
#include "Porcupine.hpp"
#include <cinttypes>
#include <sstream>
#include "raft/Raft.hpp"
#include "raft/Channel.hpp"
#include "raft/SyncChannel.hpp"
#include "KVTestFramework/NetworkConfig.hpp"
#include "KVTestFramework/ProxyService.hpp"
KVTestFramework::KVTestFramework(const std::string& a, const std::string& t, NetworkConfig& c, raft::Channel& l, raft::Channel& f, raft::Raft& r, zdb::RetryPolicy p)
    : addr {a},
      targetServerAddr {t},
      networkConfig(c),
      mem {zdb::InMemoryKVStore {}},
      leader{l},
      follower{f},
      raft {r},
      kvState {mem, leader, follower, raft},
      targetService {kvState},
      targetProxyService{targetServerAddr, networkConfig, p, getDefaultKVProxyFunctions()},
      service {targetProxyService},
      targetServer {targetServerAddr, targetService},
      server {addr, service},
      rng(std::random_device{}()) {
    targetProxyService.connectTarget();
}

std::vector<KVTestFramework::ClientResult> KVTestFramework::spawnClientsAndWait(
    int nClients,
    std::chrono::seconds timeout,
    std::vector<std::string> addresses,
    zdb::RetryPolicy policy,
    std::function<ClientResult(int id, zdb::KVStoreClient& client, std::atomic<bool>& done)> f
) {
    std::vector<ClientResult> results(nClients);
    std::vector<std::thread> threads;
    std::atomic<bool> done {false};
    for (int i = 0; i < nClients; ++i) {
        threads.emplace_back([&, i]() {
            zdb::Config config {addresses, policy};
            zdb::KVStoreClient client {config};
            results[i] = f(i, client, done);
        });
    }
    std::this_thread::sleep_for(timeout);
    done.store(true);
    for (auto& thread : threads) {
        thread.join();
    }
    return results;
}

KVTestFramework::ClientResult KVTestFramework::oneClientSet(
    int clientId,
    zdb::KVStoreClient& client,
    std::vector<zdb::Key> keys,
    bool randomKeys,
    std::atomic<bool>& done
) {
    std::uniform_int_distribution<int> dist {0, static_cast<int>(keys.size()) - 1};
    ClientResult result {0, 0};
    std::unordered_map<zdb::Key, int, zdb::KeyHash> versions{};
    for (const auto& key : keys) {
        versions[key] = 0;
    }
    while(!done.load()) {
        auto key = keys[0];
        if (randomKeys) {
            key = keys[dist(rng)];
        }
        auto [version, ok] = oneSet(clientId, client, key, versions[key]);
        if (ok) {
            result.nOK++;
        } else {
            result.nMaybe++;
        }
        versions[key] = version;
    }
    return result;
}

std::pair<int, bool> KVTestFramework::oneSet(
    int clientId,
    zdb::KVStoreClient& client,
    zdb::Key key,
    uint64_t version
) {
    auto data = std::to_string(clientId) + " " + std::to_string(version);
    while (true) {
        auto status = setJson(clientId, client, key, zdb::Value{data, version});
        if (!(status.has_value() ||
              status.error().code == zdb::ErrorCode::VersionMismatch ||
              status.error().code == zdb::ErrorCode::Maybe)) {
            throw std::runtime_error("oneSet: wrong error " + status.error().what);
        }
        zdb::Value getValue = getJson(clientId, client, key);
        if (status.has_value() && getValue.version == version + 1) {
            std::stringstream ss(getValue.data);
            int getClientId;
            ss >> getClientId;
            if (clientId != getClientId || getValue.data != data) {
                throw std::runtime_error("oneSet: wrong value " + getValue.data);
            }
        }
        if (status.has_value() || status.error().code == zdb::ErrorCode::Maybe) {
            return {getValue.version, status.has_value()};
        }
        version = getValue.version;
    }
}

std::expected<std::monostate, zdb::Error> KVTestFramework::setJson(
    int clientId,
    zdb::KVStoreClient& client,
    zdb::Key key,
    zdb::Value value
) {
    auto start = std::chrono::steady_clock::now();
    auto result = client.set(key, value);
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration duration = end - start;
    porcupine.append(
        Porcupine::Operation{
            .input = Porcupine::Input{
                .op = Porcupine::SET_OP,
                .key = key.data,
                .value = value.data,
                .version = value.version
            },
            .output = Porcupine::Output{
                .value = value.data,
                .version = value.version,
                .error = result.has_value() ? zdb::ErrorCode::OK : result.error().code
            },
            .start = start,
            .end = end,
            .clientId = clientId
        }
    );
    return result;
}

zdb::Value KVTestFramework::getJson(
    int clientId,
    zdb::KVStoreClient& client,
    zdb::Key key
) {
    auto start = std::chrono::steady_clock::now();
    auto result = client.get(key);
    auto end = std::chrono::steady_clock::now();
    porcupine.append(
        Porcupine::Operation{
            .input = Porcupine::Input{
                .op = Porcupine::GET_OP,
                .key = key.data,
                .value = "",
                .version = 0
            },
            .output = Porcupine::Output{
                .value = result.has_value() ? result.value().data : "",
                .version = result.has_value() ? result.value().version : 0,
                .error = result.has_value() ? zdb::ErrorCode::OK : result.error().code
            },
            .start = start,
            .end = end,
            .clientId = clientId
        }
    );
    return result.has_value() ? result.value() : zdb::Value{"", 0};
}

bool KVTestFramework::checkSetConcurrent(
    zdb::KVStoreClient& client,
    zdb::Key key,
    std::vector<ClientResult> results
) {
    auto value = getJson(-1, client, key);
    int totalOK = 0;
    int totalMaybe = 0;
    for (const auto& result : results) {
        totalOK += result.nOK;
        totalMaybe += result.nMaybe;
    }
    if (networkConfig.isReliable()) {
        return value.version == totalOK;
    } else {
        return value.version <= totalOK + totalMaybe;
    }
}

KVTestFramework::~KVTestFramework() {
}
