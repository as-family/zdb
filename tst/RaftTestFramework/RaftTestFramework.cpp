#include "RaftTestFramework/RaftTestFramework.hpp"
#include <unordered_map>
#include <vector>
#include <string>
#include <utility>
#include "KVTestFramework/NetworkConfig.hpp"
#include "KVTestFramework/KVTestFramework.hpp"
#include "raft/Raft.hpp"
#include "raft/Channel.hpp"
#include "raft/SyncChannel.hpp"
#include "raft/RaftImpl.hpp"
#include <algorithm>
#include "common/Command.hpp"
#include "raft/Command.hpp"
#include "common/Command.hpp"
#include "proto/types.pb.h"
#include <thread>
#include <chrono>
#include "proto/raft.grpc.pb.h"
#include "common/Util.hpp"

RAFTTestFramework::RAFTTestFramework(
        std::vector<EndPoints>& c,
        zdb::RetryPolicy p
    ) : config(c), policy{p}, gen{random_generator()} {
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
            leaders.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftTarget), std::forward_as_tuple());
            followers.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftTarget), std::forward_as_tuple());
            rafts.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftTarget), std::forward_as_tuple(ps, e.raftProxy, leaders.at(e.raftTarget), followers.at(e.raftTarget), policy, [this, &e](const std::string addr, zdb::RetryPolicy) -> Client& {
                clients[e.raftTarget].emplace(std::piecewise_construct, std::forward_as_tuple(addr), std::forward_as_tuple(addr, e.raftNetworkConfig, policy));
                return clients[e.raftTarget].at(addr);
            }));
            raftServices.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftTarget), std::forward_as_tuple(rafts.at(e.raftTarget)));
            raftServers.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftTarget), std::forward_as_tuple(e.raftTarget, raftServices.at(e.raftTarget)));
            proxies.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftTarget), std::forward_as_tuple(e.raftTarget, e.raftNetworkConfig, policy));
            raftProxies.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftTarget), std::forward_as_tuple(proxies.at(e.raftTarget)));
            raftProxyServers.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftTarget), std::forward_as_tuple(e.raftProxy, raftProxies.at(e.raftTarget)));
            kvTests.emplace(std::piecewise_construct, std::forward_as_tuple(e.raftTarget), std::forward_as_tuple(e.kvProxy, e.kvTarget, e.kvNetworkConfig, leaders.at(e.raftTarget), followers.at(e.raftTarget), rafts.at(e.raftTarget), policy));
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
    for (auto& [id, cs] : clients) {
        proxies.at(id).connectTarget();
        for (auto& c : cs) {
            c.second.connectTarget();
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
    std::uniform_int_distribution<> dist{0, 300};
    for (int i = 0; i < 10; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200 + dist(gen)));
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
    return rafts.stdMap();
}

KVTestFramework& RAFTTestFramework::getKVFrameworks(std::string target) {
    return kvTests.at(target);
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
    if (ts.empty()) {
        return std::nullopt;
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
        c.second.getNetworkConfig().disconnect();
    }
}

void RAFTTestFramework::connect(std::string id) {
    proxies.at(id).getNetworkConfig().connect();
    for (auto& c : clients.at(id)) {
        c.second.getNetworkConfig().connect();
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

int RAFTTestFramework::one(std::string c, int servers, bool retry) {
    auto start_time = std::chrono::steady_clock::now();
    size_t starts = 0;
    
    // Main timeout loop - try for up to 10 seconds
    while (std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::steady_clock::now() - start_time).count() < 10) {
        
        // Try all servers to find a leader that will accept the command
        int index = -1;
        for (size_t i = 0; i < rafts.size(); i++) {
            starts = (starts + 1) % rafts.size();
            
            // Get the server ID at this position
            auto it = rafts.begin();
            std::advance(it, starts);
            const std::string& server_id = it->first;
            auto& raft = it->second;
            
            // Check if this server is connected
            if (proxies.at(server_id).getNetworkConfig().isConnected()) {
                // Try to submit the command
                if (raft.start(c)) {
                    // Command was accepted, get the index where it should be committed
                    index = raft.log().lastIndex();
                    break;
                }
            }
        }
        
        if (index != -1) {
            // Someone claimed to be the leader and submitted our command
            // Wait up to 2 seconds for agreement
            auto wait_start = std::chrono::steady_clock::now();
            while (std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::steady_clock::now() - wait_start).count() < 2) {
                
                auto [nd, cmd1] = nCommitted(index);
                if (nd > 0 && nd >= servers) {
                    // Committed by enough servers
                    if (cmd1 == c) {
                        // And it was the command we submitted
                        return index;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            
            if (!retry) {
                // Not retrying, so fail if we didn't get agreement
                throw std::runtime_error{"Failed to reach agreement for command: " + c};
            }
        } else {
            // No leader found, wait a bit before trying again
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    
    // Timeout reached
    throw std::runtime_error{"Timeout: failed to reach agreement for command: " + c};
}
