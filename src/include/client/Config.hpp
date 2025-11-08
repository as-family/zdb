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
#include "proto/raft.grpc.pb.h"

namespace zdb {

using KVRPCService = RPCService<zdb::kvStore::KVStoreService>;
using KVRPCServicePtr = RPCService<zdb::kvStore::KVStoreService>*;

std::unordered_map<std::string, KVRPCService::function_t>& getDefaultKVFunctions();
std::unordered_map<std::string, typename zdb::RPCService<raft::proto::Raft>::function_t>& getDefaultRaftFunctions();

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
