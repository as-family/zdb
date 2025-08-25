#include "RaftTestFramework/RaftTestFramework.hpp"
#include <unordered_map>
#include <vector>
#include <string>
#include <utility>
#include "KVTestFramework/NetworkConfig.hpp"
#include "KVTestFramework/KVTestFramework.hpp"
#include "raft/Raft.hpp"
#include "raft/Channel.hpp"
#include "raft/RaftImpl.hpp"
#include <algorithm>
#include "common/Command.hpp"
#include "raft/Command.hpp"
#include "common/Command.hpp"
#include "proto/types.pb.h"
#include <thread>

raft::Command* commandFactory(const std::string& s) {
    auto cmd = zdb::proto::Command {};
    google::protobuf::Any any;
    if (any.ParseFromString(s)) {
        if (any.UnpackTo(&cmd)) {
            if (cmd.op() == "get") {
                return new zdb::Get {cmd};
            } else if (cmd.op() == "put") {
                return new zdb::Set {cmd};
            } else {
                return nullptr;
            }
        }
    }
}

RAFTTestFramework::RAFTTestFramework(
        std::vector<std::tuple<std::string, std::string, NetworkConfig>> c
    ) : config(c) {
    std::vector<std::string> proxies {config.size()};
    std::transform(
        config.begin(),
        config.end(),
        proxies.begin(),
        [](const auto& tup) { return std::get<1>(tup); }
    );
    std::vector<std::thread> threads{config.size()};
    for (auto& [target, proxy, cfg] : config) {
        threads.emplace_back(
            [this, target, proxy, cfg, proxies]() mutable {
                channels.emplace(std::piecewise_construct, std::forward_as_tuple(target), std::forward_as_tuple());
                rafts.emplace(std::piecewise_construct, std::forward_as_tuple(target), std::forward_as_tuple(proxies, proxy, channels.at(target), &commandFactory));
                kvTests.emplace(std::piecewise_construct, std::forward_as_tuple(target), std::forward_as_tuple(proxy, target, cfg, &rafts.at(target), &channels.at(target)));
            }
        );
    }
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}

int RAFTTestFramework::nRole(raft::Role role) {
    int count = 0;
    for (const auto& raft : rafts) {
        if (raft.second.getRole() == role) {
            count++;
        }
    }
    return count;
}

bool RAFTTestFramework::check1Leader() {
    return nRole(raft::Role::Leader) == 1 &&
           nRole(raft::Role::Follower) == rafts.size() - 1 &&
           nRole(raft::Role::Candidate) == 0;
}

std::unordered_map<std::string, raft::RaftImpl>& RAFTTestFramework::getRafts() {
    return rafts;
}

RAFTTestFramework::~RAFTTestFramework() = default;


