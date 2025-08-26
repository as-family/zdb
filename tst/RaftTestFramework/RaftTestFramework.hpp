#ifndef RAFT_TEST_FRAMEWORK_H
#define RAFT_TEST_FRAMEWORK_H

#include <unordered_map>
#include <vector>
#include <string>
#include <utility>
#include "KVTestFramework/NetworkConfig.hpp"
#include "KVTestFramework/KVTestFramework.hpp"
#include "raft/RaftImpl.hpp"
#include "raft/Channel.hpp"
#include "RaftTestFramework/ProxyRaftService.hpp"
#include "KVTestFramework/ProxyService.hpp"
#include "proto/raft.grpc.pb.h"
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
    RAFTTestFramework(
        std::vector<EndPoints>& c,
        zdb::RetryPolicy p
    );
    bool check1Leader();
    int nRole(raft::Role role);
    std::unordered_map<std::string, raft::RaftImpl>& getRafts();
    void start();
    ~RAFTTestFramework();
private:
    std::vector<EndPoints>& config;
    std::unordered_map<std::string, raft::Channel> channels;
    std::unordered_map<std::string, raft::RaftImpl> rafts;
    std::unordered_map<std::string, raft::RaftServiceImpl> raftServices;
    std::unordered_map<std::string, ProxyService<raft::proto::Raft>> proxies;
    std::unordered_map<std::string, ProxyRaftService> raftProxies;
    std::unordered_map<std::string, zdb::RPCServer<ProxyRaftService>> raftProxyServers;
    std::unordered_map<std::string, zdb::RPCServer<raft::RaftServiceImpl>> raftServers;
    std::unordered_map<std::string, KVTestFramework> kvTests;
    std::vector<std::thread> serverThreads;
    std::mutex m1, m2, m3, m4, m5, m6, m7, m8;
    zdb::RetryPolicy policy;
};

#endif // RAFT_TEST_FRAMEWORK_H
