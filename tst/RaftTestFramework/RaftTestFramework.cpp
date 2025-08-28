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
#include "common/Util.hpp"

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
    throw std::runtime_error{"could not deserialize command "};
}

RAFTTestFramework::RAFTTestFramework(
        std::vector<EndPoints>& c,
        zdb::RetryPolicy p
    ) : config(c), serverThreads{}, policy{p}, gen{random_generator()} {
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
            rafts.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftTarget), std::forward_as_tuple(ps, e.raftProxy, channels.at(e.raftTarget), policy, &commandFactory, [this, &e] (const std::string addr, zdb::RetryPolicy) -> Client* {
                auto t = new Client{addr, e.raftNetworkConfig};
                clients[e.raftTarget].push_back(t);
                return t;
            }));
            l2.unlock();
            std::unique_lock l3{m3};
            raftServices.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftTarget), std::forward_as_tuple(&rafts.at(e.raftTarget)));
            l3.unlock();
            std::unique_lock l4{m4};
            raftServers.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftTarget), std::forward_as_tuple(e.raftTarget, raftServices.at(e.raftTarget)));
            l4.unlock();
            std::unique_lock l5{m5};
            proxies.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftTarget), std::forward_as_tuple(e.raftTarget, e.raftNetworkConfig));
            l5.unlock();
            std::unique_lock l6{m6};
            raftProxies.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftTarget), std::forward_as_tuple(proxies.at(e.raftTarget)));
            l6.unlock();
            std::unique_lock l7{m7};
            raftProxyServers.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftTarget), std::forward_as_tuple(e.raftProxy, raftProxies.at(e.raftTarget)));
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
    for (auto& [id, client] : clients) {
        proxies.at(id).connectTarget();
        for (auto& c : client) {
            c->connectTarget();
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

std::string RAFTTestFramework::check1Leader() {
    std::uniform_int_distribution<> dist{0, 150};
    for (int i = 0; i < 10; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(150 + dist(gen)));
        std::unordered_map<uint64_t, std::vector<std::string>> termsMap{};
        for (const auto& [id, raft] : rafts) {
            if (proxies.at(id).getNetworkConfig().isConnected()) {
                if (raft.getRole() == raft::Role::Leader) {
                    termsMap[raft.getCurrentTerm()].push_back(id);
                }
            }
        }
        uint64_t lastTermWithLeader = 0;
        for (const auto& [term, ids] : termsMap) {
            if (ids.size() > 1) {
                throw std::runtime_error{"More than one leader in term " + std::to_string(term)};
            }
            if (term > lastTermWithLeader) {
                lastTermWithLeader = term;
            }
        }
        if (lastTermWithLeader != 0) {
            return termsMap.at(lastTermWithLeader)[0];
        }
    }
    throw std::runtime_error{"No leader found"};
}

std::unordered_map<std::string, raft::RaftImpl<RAFTTestFramework::Client>>& RAFTTestFramework::getRafts() {
    return rafts;
}

std::vector<uint64_t> RAFTTestFramework::terms() {
    std::vector<uint64_t> ts(rafts.size());
    std::transform(
        rafts.begin(),
        rafts.end(),
        ts.begin(),
        [](const auto& pair) { return pair.second.getCurrentTerm(); }
    );
    return ts;
}

std::optional<uint64_t> RAFTTestFramework::checkTerms() {
    std::vector<uint64_t> ts;
    for (auto& [id, raft] : rafts) {
        if (proxies.at(id).getNetworkConfig().isConnected()) {
            ts.push_back(raft.getCurrentTerm());
        }
    }
    if(std::all_of(ts.begin(), ts.end(), [ts](uint64_t t) { return t == ts[0]; })) {
        return ts[0];
    }
    return std::nullopt;
}

bool RAFTTestFramework::checkNoLeader() {
    std::uniform_int_distribution<> dist{0, 100};
    for (auto& [id, raft] : rafts) {
        if (proxies.at(id).getNetworkConfig().isConnected()) {
            if (raft.getRole() == raft::Role::Leader) {
                return false;
            }
        }
    }
    return true;
}

void RAFTTestFramework::disconnect(std::string id) {
    proxies.at(id).getNetworkConfig().disconnect();
    for (auto& c : clients.at(id)) {
        c->getNetworkConfig().disconnect();
    }
}

void RAFTTestFramework::connect(std::string id) {
    proxies.at(id).getNetworkConfig().connect();
    for (auto& c : clients.at(id)) {
        c->getNetworkConfig().connect();
    }
}

RAFTTestFramework::~RAFTTestFramework() {
}
