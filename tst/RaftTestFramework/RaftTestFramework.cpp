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
#include "proto/raft.grpc.pb.h"

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
        std::vector<EndPoints>& c
    ) : config(c), serverThreads{} {
    std::vector<std::string> ps {config.size()};
    std::transform(
        config.begin(),
        config.end(),
        ps.begin(),
        [](const auto& e) { return e.raftProxy; }
    );
    for (auto& e : config) {
        channels.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftTarget), std::forward_as_tuple()); 
        rafts.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftTarget), std::forward_as_tuple(ps, e.raftProxy, channels.at(e.raftTarget), &commandFactory));
        raftServices.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftTarget), std::forward_as_tuple(&rafts.at(e.raftTarget)));
        raftServers.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftTarget), std::forward_as_tuple(e.raftTarget, raftServices.at(e.raftTarget)));
        proxies.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftProxy), std::forward_as_tuple(e.raftTarget, e.raftNetworkConfig));
        raftProxies.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftProxy), std::forward_as_tuple(proxies.at(e.raftProxy)));
        raftProxyServers.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftProxy), std::forward_as_tuple(e.raftProxy, raftProxies.at(e.raftProxy)));
        // kvTests.emplace(std::piecewise_construct, std::forward_as_tuple(e.kvTarget), std::forward_as_tuple(e.kvProxy, e.kvTarget, e.kvNetworkConfig, &rafts.at(e.raftTarget), &channels.at(e.raftTarget)));
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

RAFTTestFramework::~RAFTTestFramework() {
    // for (auto& s: raftProxyServers) {
    //     s.second.shutdown();
    // }
    // for (auto& s: raftServers) {
    //     s.second.shutdown();
    // }
    // for (auto& t : serverThreads) {
    //     if (t.joinable()) {
    //         t.join();
    //     }
    // }
}
