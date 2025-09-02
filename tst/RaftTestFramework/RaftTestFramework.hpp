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
