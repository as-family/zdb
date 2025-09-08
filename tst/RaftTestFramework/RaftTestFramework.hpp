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
#ifndef RAFT_TEST_FRAMEWORK_H
#define RAFT_TEST_FRAMEWORK_H

#include <unordered_map>
#include <vector>
#include <string>
#include <utility>
#include "KVTestFramework/NetworkConfig.hpp"
#include "KVTestFramework/KVTestFramework.hpp"
#include "raft/Channel.hpp"
#include "RaftTestFramework/ProxyRaftService.hpp"
#include "KVTestFramework/ProxyService.hpp"
#include "proto/raft.grpc.pb.h"
#include "server/RPCServer.hpp"
#include "common/Util.hpp"
#include <random>
#include "raft/SyncChannel.hpp"
#include "raft/RaftImpl.hpp"
#include "common/LockedUnorderedMap.hpp"
#include "raft/RaftServiceImpl.hpp"
#include "server/RPCServer.hpp"

struct EndPoints {
    std::string raftTarget;
    std::string raftProxy;
    std::string kvTarget;
    std::string kvProxy;
    NetworkConfig raftNetworkConfig;
    NetworkConfig kvNetworkConfig;
};

class RAFTTestFramework {
public:
    using Client = ProxyService<raft::proto::Raft>;
    RAFTTestFramework(
        std::vector<EndPoints>& c,
        zdb::RetryPolicy p
    );
    std::string check1Leader();
    bool checkNoLeader();
    int nRole(raft::Role role);
    std::vector<uint64_t> terms();
    std::optional<uint64_t> checkTerms();
    std::unordered_map<std::string, raft::RaftImpl<Client>>& getRafts();
    KVTestFramework& getKVFrameworks(std::string target);
    void disconnect(std::string);
    void connect(std::string);
    void start();
    std::pair<int, std::string> nCommitted(uint64_t index);
    int one(std::string c, int servers, bool retry);
private:
    std::vector<EndPoints>& config;
    zdb::RetryPolicy policy;
    std::mt19937 gen;
    zdb::LockedUnorderedMap<std::string, raft::SyncChannel> leaders;
    zdb::LockedUnorderedMap<std::string, raft::SyncChannel> followers;
    zdb::LockedUnorderedMap<std::string, raft::RaftImpl<Client>> rafts;
    zdb::LockedUnorderedMap<std::string, zdb::LockedUnorderedMap<std::string, Client>> clients;
    zdb::LockedUnorderedMap<std::string, raft::RaftServiceImpl> raftServices;
    zdb::LockedUnorderedMap<std::string, Client> proxies;
    zdb::LockedUnorderedMap<std::string, ProxyRaftService> raftProxies;
    zdb::LockedUnorderedMap<std::string, zdb::RPCServer<ProxyRaftService>> raftProxyServers;
    zdb::LockedUnorderedMap<std::string, zdb::RPCServer<raft::RaftServiceImpl>> raftServers;
    zdb::LockedUnorderedMap<std::string, KVTestFramework> kvTests;
};

#endif // RAFT_TEST_FRAMEWORK_H
