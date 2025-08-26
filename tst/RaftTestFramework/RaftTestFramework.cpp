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
        std::vector<EndPoints>& c,
        zdb::RetryPolicy p
    ) : config(c), serverThreads{}, policy{p} {
    std::vector<std::string> ps {config.size()};
    std::transform(
        config.begin(),
        config.end(),
        ps.begin(),
        [](const auto& e) { return e.raftProxy; }
    );
    std::vector<std::thread> threads;
    for (auto& e : config) {
        threads.emplace_back([this, &e, ps] {
            std::unique_lock l1{m1};
            channels.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftTarget), std::forward_as_tuple());
            l1.unlock();
            std::unique_lock l2{m2};
            rafts.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftTarget), std::forward_as_tuple(ps, e.raftProxy, channels.at(e.raftTarget), policy, &commandFactory));
            l2.unlock();
            std::unique_lock l3{m3};
            raftServices.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftTarget), std::forward_as_tuple(&rafts.at(e.raftTarget)));
            l3.unlock();
            std::unique_lock l4{m4};
            raftServers.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftTarget), std::forward_as_tuple(e.raftTarget, raftServices.at(e.raftTarget)));
            l4.unlock();
            std::unique_lock l5{m5};
            proxies.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftProxy), std::forward_as_tuple(e.raftTarget, e.raftNetworkConfig));
            l5.unlock();
            std::unique_lock l6{m6};
            raftProxies.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftProxy), std::forward_as_tuple(proxies.at(e.raftProxy)));
            l6.unlock();
            std::unique_lock l7{m7};
            raftProxyServers.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftProxy), std::forward_as_tuple(e.raftProxy, raftProxies.at(e.raftProxy)));
            l7.unlock();
            // kvTests.emplace(std::piecewise_construct, std::forward_as_tuple(e.kvTarget), std::forward_as_tuple(e.kvProxy, e.kvTarget, e.kvNetworkConfig, &rafts.at(e.raftTarget), &channels.at(e.raftTarget)));
        });
    }
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    start();
}

void RAFTTestFramework::start() {
    for (auto& [id, raft] : rafts) {
        raft.connectPeers();
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
