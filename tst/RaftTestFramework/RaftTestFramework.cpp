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
            leaders.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftTarget), std::forward_as_tuple());
            followers.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftTarget), std::forward_as_tuple());
            l1.unlock();
            std::unique_lock l2{m2};
            rafts.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftTarget), std::forward_as_tuple(ps, e.raftProxy, *leaders.at(e.raftTarget), *followers.at(e.raftTarget), policy, [this, &e] (const std::string addr, zdb::RetryPolicy) -> Client* {
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
            std::unique_lock l8{m8};
            kvTests[e.kvTarget] = new KVTestFramework(e.kvProxy, e.kvTarget, e.kvNetworkConfig, leaders.at(e.raftTarget), followers.at(e.raftTarget), &rafts.at(e.raftTarget));
            l8.unlock();
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

std::unordered_map<std::string, KVTestFramework*>& RAFTTestFramework::getKVFrameworks() {
    return kvTests;
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

std::pair<int, std::string> RAFTTestFramework::nCommitted(uint64_t index) {
    int count = 0;
    std::string c = "";
    for (auto& [id, raft] : rafts) {
        if (raft.log().lastIndex() >= index) {
            auto entry = raft.log().at(index);
            if (entry.has_value()) {
                count++;
                if (c == "") {
                    c = entry.value().command;
                } else {
                    if (c != entry.value().command) {
                        throw std::runtime_error{"Different commands committed at index " + std::to_string(index)};
                    }
                }
            }
        }
    }
    return {count, c};
}

RAFTTestFramework::~RAFTTestFramework() {
    for (auto& [id, kvTest] : kvTests) {
        delete kvTest;
    }
}
