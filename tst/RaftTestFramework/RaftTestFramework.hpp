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

class RAFTTestFramework {
public:
    RAFTTestFramework(
        std::vector<std::tuple<std::string, std::string, NetworkConfig>> c
    );
    ~RAFTTestFramework();
private:
    std::vector<std::tuple<std::string, std::string, NetworkConfig>> config;
    std::unordered_map<std::string, KVTestFramework> kvTests;
    std::unordered_map<std::string, raft::RaftImpl> rafts;
    std::unordered_map<std::string, raft::Channel> channels;
};

#endif // RAFT_TEST_FRAMEWORK_H
