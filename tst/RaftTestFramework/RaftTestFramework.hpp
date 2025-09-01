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
    ~RAFTTestFramework();
private:
    std::vector<EndPoints>& config;
    std::mutex m1, m2, m3, m4, m5, m6, m7, m8;
    zdb::RetryPolicy policy;
    std::mt19937 gen;
    std::unordered_map<std::string, raft::SyncChannel> leaders;
    std::unordered_map<std::string, raft::SyncChannel> followers;
    std::unordered_map<std::string, raft::RaftImpl<Client>> rafts;
    std::unordered_map<std::string, std::unordered_map<std::string, Client>> clients;
    std::unordered_map<std::string, raft::RaftServiceImpl> raftServices;
    std::unordered_map<std::string, Client> proxies;
    std::unordered_map<std::string, ProxyRaftService> raftProxies;
    std::unordered_map<std::string, zdb::RPCServer<ProxyRaftService>> raftProxyServers;
    std::unordered_map<std::string, zdb::RPCServer<raft::RaftServiceImpl>> raftServers;
    std::unordered_map<std::string, KVTestFramework> kvTests;
};

#endif // RAFT_TEST_FRAMEWORK_H
