#ifndef RAFT_HANDLE_HPP
#define RAFT_HANDLE_HPP

#include <raft/RaftImpl.hpp>
#include <raft/Channel.hpp>
#include <string>
#include <unordered_map>
#include <queue>
#include <memory>
#include "GoRPCClient.hpp"

struct RaftHandle {
    int id;
    int servers;
    std::string selfId;
    std::vector<std::string> peers;
    zdb::RetryPolicy policy;
    uintptr_t callback;
    uintptr_t channelCallback;
    std::unique_ptr<raft::Channel<std::shared_ptr<raft::Command>>> goChannel;
    std::unordered_map<std::string, int> peerIds;
    std::unordered_map<std::string, std::unique_ptr<GoRPCClient>> clients;
    std::unique_ptr<raft::RaftImpl<GoRPCClient>> raft;
};

#endif // RAFT_HANDLE_HPP
